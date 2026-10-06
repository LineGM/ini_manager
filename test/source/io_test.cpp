#include "../support/check.hpp"
#include "boost/ut.hpp"
#include "ini_manager/ini_manager.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
struct read_failure : std::streambuf
{
	std::string text = "[s]\nx=changed\n";
	std::size_t position = 0;

  protected:
	auto uflow() -> int_type override
	{
		if (position == text.size())
		{
			throw std::runtime_error("read failure");
		}
		return traits_type::to_int_type(text.at(position++));
	}
};
struct output_failure : std::streambuf
{
	enum class kind : std::uint8_t
	{
		short_write,
		throw_write,
		sync,
		throw_sync,
	} mode;
	explicit output_failure(kind name) : mode(name)
	{
	}

  protected:
	auto xsputn(const char * /*data*/, std::streamsize size) -> std::streamsize override
	{
		if (mode == kind::throw_write)
		{
			throw std::runtime_error("write failure");
		}
		return mode == kind::short_write ? size - 1 : size;
	}
	auto sync() -> int override
	{
		if (mode == kind::throw_sync)
		{
			throw std::runtime_error("flush failure");
		}
		return mode == kind::sync ? -1 : 0;
	}
};

// Deliberately no global hooks or conditional changes to the production header.
struct failing_ops
{
	ini::operation failure;
	std::filesystem::path temporary = "injected.tmp";
	bool cleanup_fails = false, created = false, closed = false, replaced = false,
		 cleaned = false;
	unsigned collisions = 0, attempts = 0;
	std::string target_bytes = "old";
	explicit failing_ops(ini::operation action) : failure(action)
	{
	}
	auto step(ini::operation action) const -> ini::result<void>
	{
		if (failure == action)
		{
			return std::unexpected(ini::detail::io_error(action));
		}
		return {};
	}
	auto inspect() const -> ini::result<void>
	{
		return step(ini::operation::inspect);
	}
	auto create() -> ini::result<bool>
	{
		if (auto outcome = step(ini::operation::create_temp); !outcome)
		{
			return std::unexpected(outcome.error());
		}
		++attempts;
		if (attempts <= collisions)
		{
			return false;
		}
		created = true;
		return true;
	}
	auto write(std::string_view /*bytes*/) const -> ini::result<void>
	{
		return step(ini::operation::write);
	}
	auto permissions() const -> ini::result<void>
	{
		return step(ini::operation::permissions);
	}
	auto close() -> ini::result<void>
	{
		closed = true;
		return step(ini::operation::close);
	}
	auto replace() -> ini::result<void>
	{
		test::require(closed);
		if (auto outcome = step(ini::operation::replace); !outcome)
		{
			return outcome;
		}
		replaced = true;
		target_bytes = "new";
		return {};
	}
	auto cleanup() noexcept -> std::error_code
	{
		cleaned = true;
		return cleanup_fails ? std::make_error_code(std::errc::permission_denied)
							 : std::error_code{};
	}
};

// Static dispatch intentionally replaces selected native operations for fault injection.
// NOLINTBEGIN(bugprone-derived-method-shadowing-base-method)
struct failing_native_ops : ini::detail::file_ops
{
	ini::operation failure;
	failing_native_ops(const std::filesystem::path &path, ini::operation action)
		: file_ops(path), failure(action)
	{
	}
	auto write(std::string_view bytes) const -> ini::result<void>
	{
		auto outcome = file_ops::write(bytes);
		if (outcome && failure == ini::operation::write)
		{
			return std::unexpected(ini::detail::io_error(failure));
		}
		return outcome;
	}
	auto close() -> ini::result<void>
	{
		auto outcome = file_ops::close();
		if (outcome && failure == ini::operation::close)
		{
			return std::unexpected(ini::detail::io_error(failure));
		}
		return outcome;
	}
	auto replace() const -> ini::result<void>
	{
		if (failure == ini::operation::replace)
		{
			return std::unexpected(ini::detail::io_error(failure));
		}
		return file_ops::replace();
	}
};

// NOLINTEND(bugprone-derived-method-shadowing-base-method)

} // namespace
namespace
{
using boost::ut::expect;
using boost::ut::throws;
using test::must;

void test_normal_eof_is_successful_even_without_final_newline()
{
	for (const auto *text : {"", "[another_section]", "[s]\nx=y", "[s]\nx=y\n"})
	{
		std::istringstream input(text);
		ini::ini_manager config;
		expect(static_cast<bool>(input >> config));
		expect(input.eof());
		expect(!input.fail());
	}
}

void test_preexisting_stream_errors_are_not_cleared()
{
	for (const auto state :
		 {std::ios::failbit, std::ios::badbit, std::ios::failbit | std::ios::eofbit})
	{
		auto config = test::parse("[old]\nx=old");
		std::istringstream input("[new]\nx=new");
		input.setstate(state);
		expect(!config.load_stream(input));
		expect((input.rdstate() & state) == state);
		expect(config.get_sections() == std::vector<std::string>{"old"});
	}
}

void test_i_o_exceptions_become_expected_errors_and_preserve_state()
{
	for (bool const exceptions : {false, true})
	{
		auto config = test::parse("[s]\nx=old");
		const auto before = test::serialize(config);
		read_failure buffer;
		std::istream input(&buffer);
		if (exceptions)
		{
			input.exceptions(std::ios::badbit | std::ios::failbit);
		}
		auto outcome = config.load_stream(input);
		expect(!outcome);
		expect(outcome.error().op == ini::operation::read);
		expect(outcome.error().system_code == std::errc::io_error);
		expect(input.bad());
		expect(test::serialize(config) == before);
		read_failure merge_buffer;
		std::istream merge(&merge_buffer);
		expect(!config.add_from_stream(merge));
		expect(test::serialize(config) == before);
	}
}

void test_stream_operators_honor_exception_masks()
{
	ini::ini_manager config;
	std::istringstream invalid("broken");
	invalid.exceptions(std::ios::failbit);
	expect(throws<std::ios_base::failure>([&] -> void { invalid >> config; }));
	std::istringstream eof("[s]");
	eof.exceptions(std::ios::eofbit);
	expect(throws<std::ios_base::failure>([&] -> void { eof >> config; }));
	std::istringstream expected_eof("[s]");
	expected_eof.exceptions(std::ios::eofbit);
	expect(static_cast<bool>(config.load_stream(expected_eof)));
	expect(expected_eof.eof());
	std::istringstream normal("[s]");
	normal.exceptions(std::ios::failbit | std::ios::badbit);
	expect(boost::ut::nothrow([&] -> void { normal >> config; }));
	read_failure bad_buffer;
	std::istream bad(&bad_buffer);
	bad.exceptions(std::ios::badbit);
	expect(throws<std::ios_base::failure>([&] -> void { bad >> config; }));
}

void test_portable_output_and_flush_failures()
{
	auto config = test::parse("[s]\nx=y");
	for (const auto kind : {
			 output_failure::kind::short_write,
			 output_failure::kind::throw_write,
			 output_failure::kind::sync,
			 output_failure::kind::throw_sync,
		 })
	{
		for (bool const exceptions : {false, true})
		{
			output_failure buffer(kind);
			std::ostream output(&buffer);
			if (exceptions)
			{
				output.exceptions(std::ios::failbit | std::ios::badbit);
			}
			errno = ENOENT;
			auto outcome = config.write_stream(output);
			expect(!outcome);
			expect(output.fail());
			expect(outcome.error().system_code == std::errc::io_error);
			const auto expected_op = kind == output_failure::kind::sync ||
											 kind == output_failure::kind::throw_sync
										 ? ini::operation::flush
										 : ini::operation::write;
			expect(outcome.error().op == expected_op);
		}
	}
	output_failure buffer(output_failure::kind::sync);
	std::ostream output(&buffer);
	output.exceptions(std::ios::badbit);
	expect(throws<std::ios_base::failure>([&] -> void { output << config; }));
	std::ostringstream failed;
	failed.setstate(std::ios::failbit);
	expect(!config.write_stream(failed));
}

void test_atomic_save_stage_failures_preserve_old_target()
{
	for (const auto action : {
			 ini::operation::inspect,
			 ini::operation::create_temp,
			 ini::operation::write,
			 ini::operation::permissions,
			 ini::operation::close,
			 ini::operation::replace,
		 })
	{
		failing_ops ops(action);
		auto outcome = ini::detail::atomic_write(ops, "new");
		expect(!outcome);
		expect(outcome.error().op == action);
		expect(ops.target_bytes == "old");
		expect(!ops.replaced);
		expect(ops.cleaned == ops.created);
	}
	failing_ops cleanup(ini::operation::write);
	cleanup.cleanup_fails = true;
	auto outcome = ini::detail::atomic_write(cleanup, "new");
	expect(!outcome);
	expect(outcome.error().op == ini::operation::write);
	expect(outcome.error().cleanup_code == std::errc::permission_denied);
	expect(outcome.error().temporary_path == cleanup.temporary);
	failing_ops collision(ini::operation::read);
	collision.collisions = 3;
	must(ini::detail::atomic_write(collision, "new"));
	expect(collision.attempts == 4U);
	expect(collision.replaced);
	expect(!collision.cleaned);
	expect(collision.target_bytes == "new");
	failing_ops exhaustion(ini::operation::read);
	constexpr unsigned max_attempts = 128;
	exhaustion.collisions = max_attempts + 1;
	expect(!ini::detail::atomic_write(exhaustion, "new"));
	expect(exhaustion.attempts == max_attempts);
	expect(!exhaustion.cleaned);
}

void test_file_transactions_paths_permissions_and_cleanup()
{
	test::temp_directory const dir;
	const auto path = dir.path / std::filesystem::path(u8"настройки.ini");
	test::write(path, "[s]\nx=old\n");
	auto config = must(ini::ini_manager::from_file(path));
	expect(config.file_path() == path);
	const auto old_path = config.file_path();
	const auto old_data = test::serialize(config);
	auto failed = config.load_file(dir.path / "missing.ini");
	expect(!failed);
	expect(failed.error().system_code == std::errc::no_such_file_or_directory);
	expect(config.file_path() == old_path);
	expect(test::serialize(config) == old_data);
	const auto invalid = dir.path / "invalid.ini";
	test::write(invalid, "[s]\nx=changed\nbroken");
	expect(!config.load_file(invalid));
	expect(!config.add_from_file(invalid));
	expect(config.file_path() == old_path);
	expect(test::serialize(config) == old_data);
	const auto extra = dir.path / "extra.ini";
	test::write(extra, "[s]\nx=merged\n[empty]");
	must(config.add_from_file(extra));
	expect(config.file_path() == old_path);
	must(config.set_value({"s"}, {"x"}, "new"));
#ifndef _WIN32
	const auto mode = std::filesystem::perms::owner_read |
					  std::filesystem::perms::owner_write |
					  std::filesystem::perms::group_read;
	std::filesystem::permissions(path, mode);
#endif
	must(config.write_file());
	expect(test::same_data(config, must(ini::ini_manager::from_file(path))));
#ifndef _WIN32
	expect((std::filesystem::status(path).permissions() & std::filesystem::perms::all) ==
		   mode);
#endif
	const auto fresh = dir.path / "fresh.ini";
	must(config.write_file(fresh));
	expect(config.file_path() == old_path);
#ifndef _WIN32
	expect((std::filesystem::status(fresh).permissions() & std::filesystem::perms::all) ==
		   (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
#endif
	expect(!config.write_file(dir.path));
	expect(!config.write_file(dir.path / "absent" / "file.ini"));
	std::error_code code;
	const auto link = dir.path / "link.ini";
	std::filesystem::create_symlink(path, link, code);
	if (!code)
	{
		const auto before = test::read(path);
		expect(!config.write_file(link));
		expect(test::read(path) == before);
	}
	for (const auto &entry : std::filesystem::directory_iterator(dir.path))
	{
		expect(!entry.path().filename().string().starts_with(".ini-manager-"));
	}
	std::istringstream reload("[new]");
	must(config.load_stream(reload));
	expect(!config.file_path());
	expect(!config.write_file());
	const std::filesystem::path nul(std::string("bad\0path", 8));
	expect(!config.write_file(nul));
	expect(!config.load_file(nul));
#ifdef __linux__
	expect(!config.write_file("/dev/full"));
	std::ofstream full("/dev/full");
	expect(!config.write_stream(full));
#endif
}

void test_stream_buffers_preserve_available_system_error_codes()
{
	struct coded_input : std::streambuf
	{
	  protected:
		auto uflow() -> int_type override
		{
			throw std::system_error(std::make_error_code(std::errc::permission_denied));
		}
	} in_buffer;
	struct coded_output : std::streambuf
	{
	  protected:
		auto xsputn(const char * /*data*/, std::streamsize /*size*/)
			-> std::streamsize override
		{
			throw std::ios_base::failure(
				"disk full", std::make_error_code(std::errc::no_space_on_device));
		}
	} out_buffer;
	std::istream input(&in_buffer);
	std::ostream output(&out_buffer);
	auto config = test::parse("[s]\nx=original");
	auto read = config.load_stream(input);
	expect(!read);
	expect(read.error().system_code == std::errc::permission_denied);
	auto write = config.write_stream(output);
	expect(!write);
	expect(write.error().system_code == std::errc::no_space_on_device);
}

void test_native_staged_failures_retain_real_target_bytes()
{
	test::temp_directory const dir;
	const auto target = dir.path / "config.ini";
	test::write(target, "original bytes");
	for (const auto action :
		 {ini::operation::write, ini::operation::close, ini::operation::replace})
	{
		failing_native_ops operations(target, action);
		auto outcome = ini::detail::atomic_write(operations, "replacement bytes");
		expect(!outcome);
		expect(outcome.error().op == action);
		expect(test::read(target) == "original bytes");
		expect(!std::filesystem::exists(operations.temporary));
	}
	ini::detail::native_file collision;
	expect(!collision.open(target, /*writing=*/true));
	expect(test::read(target) == "original bytes");
	std::error_code code;
	const auto link = dir.path / "collision-link";
	std::filesystem::create_symlink(target, link, code);
	if (!code)
	{
		expect(!collision.open(link, /*writing=*/true));
		expect(test::read(target) == "original bytes");
	}
}

void test_allocation_exceptions_from_stream_buffers_propagate()
{
	struct bad_input : std::streambuf
	{
	  protected:
		auto uflow() -> int_type override
		{
			throw std::bad_alloc();
		}
	} input_buffer;
	struct bad_output : std::streambuf
	{
	  protected:
		auto xsputn(const char * /*data*/, std::streamsize /*size*/)
			-> std::streamsize override
		{
			throw std::bad_alloc();
		}
	} output_buffer;
	auto config = test::parse("[s]\nx=original");
	std::istream input(&input_buffer);
	std::ostream output(&output_buffer);
	expect(throws<std::bad_alloc>(
		[&] -> void { [[maybe_unused]] const auto result = config.load_stream(input); }));
	expect(must(config.get_value({"s"}, {"x"})) == "original");
	expect(throws<std::bad_alloc>([&] -> void {
		[[maybe_unused]] const auto result = config.write_stream(output);
	}));
}

void test_validation_finishes_before_touching_target()
{
	test::temp_directory const dir;
	const auto path = dir.path / "original.ini";
	test::write(path, "untouched");
	ini::parse_options options;
	options.max_line_bytes = 3;
	const auto config = test::parse("a=b", options);
	expect(!config.write_file(path));
	expect(test::read(path) == "untouched");
}
} // namespace

auto main() -> int
try
{
	using boost::ut::operator""_test;
	"normal EOF is successful even without final newline"_test =
		test_normal_eof_is_successful_even_without_final_newline;
	"preexisting stream errors are not cleared"_test =
		test_preexisting_stream_errors_are_not_cleared;
	"I/O exceptions become expected errors and preserve state"_test =
		test_i_o_exceptions_become_expected_errors_and_preserve_state;
	"stream operators honor exception masks"_test =
		test_stream_operators_honor_exception_masks;
	"portable output and flush failures"_test = test_portable_output_and_flush_failures;
	"atomic save stage failures preserve old target"_test =
		test_atomic_save_stage_failures_preserve_old_target;
	"file transactions paths permissions and cleanup"_test =
		test_file_transactions_paths_permissions_and_cleanup;
	"stream buffers preserve available system error codes"_test =
		test_stream_buffers_preserve_available_system_error_codes;
	"native staged failures retain real target bytes"_test =
		test_native_staged_failures_retain_real_target_bytes;
	"allocation exceptions from stream buffers propagate"_test =
		test_allocation_exceptions_from_stream_buffers_propagate;
	"validation finishes before touching target"_test =
		test_validation_finishes_before_touching_target;
}

catch (const std::exception &error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
