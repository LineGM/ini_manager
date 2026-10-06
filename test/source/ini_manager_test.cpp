#include "../support/check.hpp"
#include "boost/ut.hpp"
#include "ini_manager/ini_manager.hpp"
#include <concepts>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
struct user_value
{
	int value = 0;
	friend auto operator>>(std::istream &input, user_value &value) -> std::istream &
	{
		return input >> value.value;
	}
};
struct throwing_value
{
	friend auto operator>>(std::istream & /*input*/, throwing_value & /*value*/)
		-> std::istream &
	{
		throw std::runtime_error("conversion");
	}
};
struct throwing_format
{
};
} // namespace
template <> struct std::formatter<throwing_format> : std::formatter<std::string_view>
{
	template <class Context>
	auto format(const throwing_format & /*value*/, Context & /*context*/) const
		-> Context::iterator
	{
		throw std::runtime_error("formatter");
	}
};
namespace
{
struct unsupported
{
};
template <class T>
concept has_subscript = requires(T &value) { value["s"]; };
static_assert(!has_subscript<ini::ini_manager>);
static_assert(!ini::readable_value<unsupported>);
static_assert(!ini::readable_value<const char *>);
static_assert(!ini::readable_value<std::string_view>);
static_assert(ini::readable_value<user_value>);
static_assert(!noexcept(std::declval<const ini::ini_manager &>().get_value<int>({"s"},
																				{"k"})));
static_assert(std::same_as<decltype(std::declval<ini::ini_manager>().get_value_or_default(
							   {"s"}, {"k"}, "literal")),
						   ini::result<std::string>>);

static_assert(ini::detail::valid_section("server"));
static_assert(!ini::detail::valid_section("[server]"));
static_assert(ini::detail::valid_key("port", {}));
static_assert(!ini::detail::valid_key("port=", {}));
static_assert(ini::detail::valid_value("data;literal", {.inline_comments = true}));
static_assert(!ini::detail::valid_value("data ;comment", {.inline_comments = true}));

} // namespace
namespace
{
using boost::ut::expect;
using boost::ut::throws;
using test::must;

void test_value_semantics_and_owning_results()
{
	auto original = test::parse("[s]\nx=original\n");
	const auto snapshot = must(original.get_value({"s"}, {"x"}));
	const auto sections = original.get_sections();
	auto copy = original;
	must(copy.set_value({"s"}, {"x"}, "copy"));
	expect(must(original.get_value({"s"}, {"x"})) == "original");
	ini::ini_manager assigned;
	assigned = original;
	must(assigned.set_value({"s"}, {"x"}, "assigned"));
	expect(must(original.get_value({"s"}, {"x"})) == "original");
	std::istringstream reload("[other]\nx=reloaded");
	must(original.load_stream(reload));
	expect(snapshot == "original");
	expect(sections == std::vector<std::string>{"s"});
	expect(must(copy.get_value({"s"}, {"x"})) == "copy");
	ini::ini_manager moved(std::move(copy));
	// The moved-from empty state is a documented contract.
	// NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved)
	expect(copy.get_sections().empty());
	expect(!copy.file_path());
	must(copy.set_value({"new"}, {"x"}, 1));
	assigned = std::move(moved);
	// NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved)
	expect(moved.get_sections().empty());
	expect(must(assigned.get_value({"s"}, {"x"})) == "copy");
	auto temporary_result = test::parse("[s]\nx=owning").get_value({"s"}, {"x"});
	expect(must(std::move(temporary_result)) == "owning");
}

void test_borrowed_substrings_are_copied_only_when_inserted()
{
	ini::ini_manager config;
	std::string names = "!server!port!";
	const ini::section group{std::string_view(names).substr(1, 6)};
	const ini::key field{
		std::string_view(names).substr(names.find("port"),
									   std::string_view("port").size()),
	};
	constexpr int initial_port = 80;
	constexpr int changed_port = 9000;
	must(config.set_section(group));
	must(config.set_value(group, field, initial_port));
	must(config.set_value(group, field, changed_port));
	expect(config.get_sections() == std::vector<std::string>{"server"});
	expect(config.get_keys(group) == std::vector<std::string>{"port"});
	names.assign(names.size(), 'x');
	expect(must(config.get_value<int>({"server"}, {"port"})) == changed_port);
}

void test_diagnostics_tolerate_unrecognized_public_enum_values()
{
	constexpr auto unknown = std::numeric_limits<std::uint8_t>::max();
	ini::error diagnostic;
	// Intentionally exercise unknown values of the fixed uint8_t underlying type.
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	diagnostic.op = static_cast<ini::operation>(unknown);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	diagnostic.reason = static_cast<ini::error_reason>(unknown);
	expect(diagnostic.message() == "unknown operation: unknown reason");
}

void test_default_only_for_missing_data()
{
	constexpr int fallback_number = 8;
	const auto config = test::parse("[s]\nx=actual\nnumber=bad\nempty=\n");
	expect(must(config.get_value_or_default({"s"}, {"x"}, "fallback")) == "actual");
	expect(must(config.get_value_or_default({"s"}, {"x"}, std::string("fallback"))) ==
		   "actual");
	expect(must(config.get_value_or_default({"s"}, {"x"},
											std::string_view("fallback"))) == "actual");
	expect(must(config.get_value_or_default({"s"}, {"absent"}, "fallback")) ==
		   "fallback");
	expect(must(config.get_value_or_default({"absent"}, {"x"}, fallback_number)) ==
		   fallback_number);
	expect(must(config.get_value_or_default({"s"}, {"empty"}, "fallback")).empty());
	expect(
		config.get_value_or_default({"s"}, {"number"}, fallback_number).error().reason ==
		ini::error_reason::invalid_format);
	expect(config.get_value({"absent"}, {"x"}).error().reason ==
		   ini::error_reason::missing_section);
	expect(config.get_value({"s"}, {"absent"}).error().reason ==
		   ini::error_reason::missing_key);
}

void test_string_arguments_preserve_array_lengths_and_reject_null_pointers()
{
	ini::ini_manager config;
	// A raw array without a terminator exercises the public array overload.
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
	const char raw[] = {'a', 'b'};
	must(config.set_value({"s"}, {"x"}, raw));
	expect(must(config.get_value({"s"}, {"x"})) == "ab");
	expect(!config.set_value({"s"}, {"x"}, "a\0b"));
	const char *null = nullptr;
	expect(!config.set_value({"s"}, {"x"}, null));
	expect(!config.get_value_or_default({"s"}, {"missing"}, null));
	expect(must(config.get_value_or_default({"s"}, {"missing"}, "a\0b")) ==
		   std::string("a\0b", 3));
	expect(must(config.get_value_or_default({"s"}, {"missing"}, raw)) == "ab");
}

void test_exceptions_in_user_conversions_propagate_without_mutation()
{
	constexpr int expected_value = 42;
	auto config = test::parse("[s]\nx=42\n");
	expect(throws<std::runtime_error>([&] -> void {
		[[maybe_unused]] const auto result =
			config.get_value<throwing_value>({"s"}, {"x"});
	}));
	expect(throws<std::runtime_error>([&] -> void {
		[[maybe_unused]] const auto result =
			config.set_value({"s"}, {"x"}, throwing_format{});
	}));
	expect(must(config.get_value<int>({"s"}, {"x"})) == expected_value);
	expect(must(config.get_value<user_value>({"s"}, {"x"})).value == expected_value);
	must(config.set_value({"s"}, {"x"}, "42junk"));
	expect(!config.get_value<user_value>({"s"}, {"x"}));
}

void test_integer_ranges_and_full_consumption()
{
	ini::ini_manager config;
	for (const auto *bad : {"-1", "+1", "0x10", "123junk", "", "1 2"})
	{
		must(config.set_value({"s"}, {"x"}, bad));
		expect(!config.get_value<unsigned>({"s"}, {"x"})) << bad;
	}
	must(config.set_value({"s"}, {"x"}, "999999999999999999999999999999999"));
	expect(config.get_value<int>({"s"}, {"x"}).error().reason ==
		   ini::error_reason::out_of_range);
	for (int const value :
		 {std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0})
	{
		must(config.set_value({"s"}, {"x"}, value));
		expect(must(config.get_value<int>({"s"}, {"x"})) == value);
	}
	must(config.set_value({"s"}, {"x"}, std::numeric_limits<std::int8_t>::min()));
	expect(must(config.get_value<std::int8_t>({"s"}, {"x"})) ==
		   std::numeric_limits<std::int8_t>::min());
	must(config.set_value({"s"}, {"x"}, std::numeric_limits<std::uint8_t>::max()));
	expect(must(config.get_value<std::uint8_t>({"s"}, {"x"})) ==
		   std::numeric_limits<std::uint8_t>::max());
	expect(config.get_value<std::int8_t>({"s"}, {"x"}).error().reason ==
		   ini::error_reason::out_of_range);
	must(config.set_value({"s"}, {"x"}, "256"));
	expect(!config.get_value<std::uint8_t>({"s"}, {"x"}));
}

void test_floating_conversions_are_finite_exact_and_locale_independent()
{
	struct comma : std::numpunct<char>
	{
	  protected:
		auto do_decimal_point() const -> char override
		{
			return ',';
		}
	};
	struct restore_locale
	{
		std::locale old = std::locale();
		restore_locale() = default;
		restore_locale(const restore_locale &) = delete;
		restore_locale(restore_locale &&) = delete;
		auto operator=(const restore_locale &) -> restore_locale & = delete;
		auto operator=(restore_locale &&) -> restore_locale & = delete;
		~restore_locale()
		{
			std::locale::global(old);
		}
	};
	const restore_locale restore;
	std::locale::global(std::locale(std::locale::classic(), new comma));
	ini::ini_manager config;
	const auto roundtrip = [&]<class T> -> auto {
		for (const T value : {
				 T{0},
				 -T{0},
				 T{1.25},
				 std::numeric_limits<T>::max(),
				 std::numeric_limits<T>::min(),
				 std::numeric_limits<T>::denorm_min(),
			 })
		{
			must(config.set_value({"s"}, {"x"}, value));
			const auto read = must(config.get_value<T>({"s"}, {"x"}));
			expect(read == value);
			expect(std::signbit(read) == std::signbit(value));
			const auto reread = test::parse(test::serialize(config));
			expect(must(reread.get_value<T>({"s"}, {"x"})) == value);
		}
	};
	roundtrip.template operator()<float>();
	roundtrip.template operator()<double>();
	// The API promises long double precision; narrowing it would weaken this test.
	// NOLINTNEXTLINE(google-runtime-float)
	roundtrip.template operator()<long double>();
	for (const auto *bad :
		 {"+1.0", "1,25", "1.0junk", "nan", "NaN", "inf", "-infinity", "0x1p2"})
	{
		must(config.set_value({"s"}, {"x"}, bad));
		expect(!config.get_value<double>({"s"}, {"x"})) << bad;
	}
	for (const auto *bad : {"1e9999", "1e-9999"})
	{
		must(config.set_value({"s"}, {"x"}, bad));
		expect(config.get_value<double>({"s"}, {"x"}).error().reason ==
			   ini::error_reason::out_of_range);
	}
	expect(!config.set_value({"s"}, {"x"}, std::numeric_limits<double>::infinity()));
	expect(!config.set_value({"s"}, {"x"}, std::numeric_limits<double>::quiet_NaN()));
}

void test_ascii_bool_and_single_byte_char()
{
	ini::ini_manager config;
	for (const auto *text : {"true", "TRUE", "TrUe", "1"})
	{
		must(config.set_value({"s"}, {"x"}, text));
		expect(must(config.get_value<bool>({"s"}, {"x"})));
	}
	for (const auto *text : {"false", "FALSE", "FaLsE", "0"})
	{
		must(config.set_value({"s"}, {"x"}, text));
		expect(!must(config.get_value<bool>({"s"}, {"x"})));
	}
	for (const auto *text : {"yes", "on", "2", "\xff"})
	{
		must(config.set_value({"s"}, {"x"}, text));
		expect(!config.get_value<bool>({"s"}, {"x"}));
	}
	must(config.set_value({"s"}, {"x"}, 'a'));
	expect(must(config.get_value<char>({"s"}, {"x"})) == 'a');
	must(config.set_value({"s"}, {"x"}, "ab"));
	expect(!config.get_value<char>({"s"}, {"x"}));
}

void test_validation_prevents_injection_and_leaves_values_unchanged()
{
	auto config = test::parse("[s]\nx=original");
	for (const auto *value :
		 {" padded", "padded ", "\tvalue", "value\nkey=bad", "a\rb", "\x01", "\x7f"})
	{
		expect(!config.set_value({"s"}, {"x"}, value));
	}
	expect(!config.set_value({"s"}, {"x"}, std::string("a\0b", 3)));
	expect(!config.set_value({"s"}, {"x"}, "a\0b"));
	expect(must(config.get_value({"s"}, {"x"})) == "original");
	for (const auto *key : {"", "a=b", "[x", ";x", "#x", " x", "x ", "x\ny"})
	{
		expect(!config.set_value({"s"}, {key}, "v"));
	}
	for (const auto *section : {"[s]", " s", "s ", "s\tx"})
	{
		expect(!config.set_section({section}));
	}
	must(config.set_section({""}));
	must(config.set_value({""}, {"a:b"}, "a;#b"));
	expect(test::same_data(config, test::parse(test::serialize(config))));
	expect(config.remove_value({"s"}, {"x"}));
	expect(!config.remove_value({"s"}, {"x"}));
	expect(config.remove_section({"s"}));
	expect(!config.remove_section({"s"}));
}
} // namespace

auto main() -> int
try
{
	using boost::ut::operator""_test;
	"value semantics and owning results"_test = test_value_semantics_and_owning_results;
	"borrowed substrings are copied only when inserted"_test =
		test_borrowed_substrings_are_copied_only_when_inserted;
	"diagnostics tolerate unrecognized public enum values"_test =
		test_diagnostics_tolerate_unrecognized_public_enum_values;
	"default only for missing data"_test = test_default_only_for_missing_data;
	"string arguments preserve array lengths and reject null pointers"_test =
		test_string_arguments_preserve_array_lengths_and_reject_null_pointers;
	"exceptions in user conversions propagate without mutation"_test =
		test_exceptions_in_user_conversions_propagate_without_mutation;
	"integer ranges and full consumption"_test = test_integer_ranges_and_full_consumption;
	"floating conversions are finite exact and locale independent"_test =
		test_floating_conversions_are_finite_exact_and_locale_independent;
	"ASCII bool and single byte char"_test = test_ascii_bool_and_single_byte_char;
	"validation prevents injection and leaves values unchanged"_test =
		test_validation_prevents_injection_and_leaves_values_unchanged;
}

catch (const std::exception &error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
