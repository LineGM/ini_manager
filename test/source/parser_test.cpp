#include "../support/check.hpp"
#include "boost/ut.hpp"
#include "ini_manager/ini_manager.hpp"
#include <cstddef>
#include <exception>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using boost::ut::expect;
using test::must;

void test_bom_comments_globals_empty_sections_and_line_endings()
{
	for (const std::string ending : {"\n", "\r\n"})
	{
		const auto input = std::format("\xEF\xBB\xBFglobal = before{0}[new] ; "
									   "comment{0}key = a;b#c{0}[empty] # comment",
									   ending);
		auto config = test::parse(input);
		expect(must(config.get_value({""}, {"global"})) == "before");
		expect(must(config.get_value({"new"}, {"key"})) == "a;b#c");
		expect(config.get_sections() == std::vector<std::string>{"", "empty", "new"});
		expect(config.get_keys({"empty"}).empty());
		expect(test::same_data(config, test::parse(test::serialize(config))));
	}
	expect(test::parse("").get_sections().empty());
	expect(test::parse(";comment\n\t#comment\n").get_sections().empty());
	expect(test::parse("[]").get_sections() == std::vector<std::string>{""});
	expect(test::parse("[another_section]").get_sections() ==
		   std::vector<std::string>{"another_section"});
}

void test_invalid_syntax_reports_one_based_position()
{
	for (const auto *bad : {"[", "[bad", "[bad]junk", "[bad];comment", "[[bad]]",
							"[bad] tail", "=empty", "missing separator"})
	{
		std::istringstream input(std::string("[old]\nx=1\n") + bad + "\ny=2");
		auto outcome = ini::ini_manager::from_stream(input);
		expect(!outcome) << bad;
		if (!outcome)
		{
			expect(outcome.error().line == 3U);
			expect(outcome.error().column.has_value());
			expect(!outcome.error().message().empty());
		}
	}
	for (const auto &bad : {std::string("[s]\nx=a\0b", 9), std::string(";comment\x01"),
							std::string("x=y\r"), std::string("x=y\ra=b")})
	{
		std::istringstream input(bad);
		auto outcome = ini::ini_manager::from_stream(input);
		expect(!outcome);
		if (!outcome)
		{
			expect(outcome.error().reason == ini::error_reason::invalid_control);
		}
	}
}

void test_duplicate_policy_and_repeated_sections()
{
	expect(test::parse("[s]\na=1\n[s]\nb=2").get_keys({"s"}).size() == 2U);
	std::istringstream input("[s]\na=1\n[s]\na=2");
	auto outcome = ini::ini_manager::from_stream(input);
	expect(!outcome);
	expect(outcome.error().reason == ini::error_reason::duplicate_key);
	expect(outcome.error().line == 4U);
	ini::parse_options options;
	options.duplicates = ini::duplicate_policy::last_wins;
	auto config = test::parse("[s]\na=1\n[s]\na=2", options);
	expect(must(config.get_value<int>({"s"}, {"a"})) == 2);
	expect(test::parse("[s]\nA=1\na=2\n[S]\na=3").get_sections().size() == 2U);
}

void test_optional_colon_and_precisely_delimited_inline_comments()
{
	ini::parse_options options;
	options.allow_colon = true;
	options.inline_comments = true;
	auto config = test::parse("[s]\na: a;b#c ; trailing\nb=abc#def\nc= "
							  "#empty\nd=x\t#comment\ne=http://host\nf=x=y",
							  options);
	expect(must(config.get_value({"s"}, {"a"})) == "a;b#c");
	expect(must(config.get_value({"s"}, {"b"})) == "abc#def");
	expect(must(config.get_value({"s"}, {"c"})).empty());
	expect(must(config.get_value({"s"}, {"d"})) == "x");
	expect(must(config.get_value({"s"}, {"e"})) == "http://host");
	expect(must(config.get_value({"s"}, {"f"})) == "x=y");
	expect(!config.set_value({"s"}, {"a:b"}, "x"));
	for (const auto *value : {";comment", "#comment", "x ;comment", "x\t#comment"})
	{
		expect(!config.set_value({"s"}, {"x"}, value));
	}
	expect(test::same_data(config, test::parse(test::serialize(config), options)));
}

void test_loads_and_merges_commit_only_complete_input()
{
	auto config = test::parse("[old]\na=original");
	const auto before = test::serialize(config);
	for (const auto *input :
		 {"[old]\na=changed\nbad", "[new]\nx=1\nx=2", "[broken\nx=bad"})
	{
		std::istringstream load(input);
		expect(!config.load_stream(load));
		expect(test::serialize(config) == before);
		std::istringstream merge(input);
		expect(!config.add_from_stream(merge));
		expect(test::serialize(config) == before);
	}
	std::istringstream merge("[old]\na=changed\nb=new\n[empty]");
	must(config.add_from_stream(merge));
	expect(must(config.get_value({"old"}, {"a"})) == "changed");
	expect(config.get_sections().size() == 2U);
	std::istringstream load("[new]\nx=1");
	must(config.load_stream(load));
	expect(config.get_sections() == std::vector<std::string>{"new"});
}

void test_finite_resource_limits_boundary_and_canonical_output()
{
	ini::parse_options options;
	options.max_line_bytes = 3;
	options.max_input_bytes = 4;
	expect(test::parse("a=b\n", options).get_sections().size() == 1U);
	for (const auto *text : {"a=bc", "a=b\n\n"})
	{
		std::istringstream input(text);
		auto outcome = ini::ini_manager::from_stream(input, options);
		expect(!outcome);
		expect(outcome.error().reason == ini::error_reason::resource_limit);
	}
	options = {};
	options.max_sections = 1;
	options.max_keys = 1;
	auto config = test::parse("[s]\na=b", options);
	expect(!config.set_section({"other"}));
	expect(!config.set_value({"s"}, {"b"}, "c"));
	must(config.set_value({"s"}, {"a"}, "replace"));
	for (const auto *text : {"[s]\n[t]", "[s]\na=1\nb=2", "a=1\n[s]"})
	{
		std::istringstream input(text);
		expect(!ini::ini_manager::from_stream(input, options));
	}
	std::istringstream merge("[t]\nx=y");
	const auto before = test::serialize(config);
	expect(!config.add_from_stream(merge));
	expect(test::serialize(config) == before);
	options = {};
	options.max_line_bytes = 3;
	config = test::parse("a=b", options);
	std::ostringstream out;
	auto outcome = config.write_stream(out);
	expect(!outcome);
	expect(out.str().empty());
	options = {};
	options.max_sections = 0;
	options.max_keys = 0;
	options.max_input_bytes = 0;
	expect(test::parse("", options).get_sections().empty());
	std::istringstream one("\n");
	expect(!ini::ini_manager::from_stream(one, options));
}

void test_bounded_reader_stops_a_generated_huge_line()
{
	struct generated : std::streambuf
	{
		std::size_t consumed = 0;

	  protected:
		auto underflow() -> int_type override
		{
			return traits_type::to_int_type('x');
		}
		auto uflow() -> int_type override
		{
			++consumed;
			return underflow();
		}
	} buffer;
	std::istream stream(&buffer);
	ini::parse_options options;
	constexpr std::size_t line_limit = 32;
	options.max_line_bytes = line_limit;
	auto outcome = ini::ini_manager::from_stream(stream, options);
	expect(!outcome);
	expect(buffer.consumed == line_limit + 1);
	expect(outcome.error().reason == ini::error_reason::resource_limit);
}

void test_generated_parse_serialize_parse_invariants()
{
	for (unsigned variant = 0; variant < 4; ++variant)
	{
		ini::parse_options options;
		options.inline_comments = (variant & 1U) != 0;
		options.allow_colon = (variant & 2U) != 0;
		ini::ini_manager config(options);
		must(config.set_section({""}));
		must(config.set_section({"empty"}));
		constexpr unsigned generated_documents = 100;
		for (unsigned i = 0; i < generated_documents; ++i)
		{
			const auto group = "section " + std::to_string(i % 7);
			const auto name = "key " + std::to_string(i);
			const auto value = "value=" + std::to_string(i) + ";literal#";
			must(config.set_value({group}, {name}, value));
		}
		auto text = test::serialize(config);
		auto reread = test::parse(text, options);
		expect(test::same_data(config, reread));
		expect(test::serialize(reread) == text);
	}
}
} // namespace

auto main() -> int
try
{
	using boost::ut::operator""_test;

	"BOM comments globals empty sections and line endings"_test =
		test_bom_comments_globals_empty_sections_and_line_endings;
	"invalid syntax reports one based position"_test =
		test_invalid_syntax_reports_one_based_position;
	"duplicate policy and repeated sections"_test =
		test_duplicate_policy_and_repeated_sections;
	"optional colon and precisely delimited inline comments"_test =
		test_optional_colon_and_precisely_delimited_inline_comments;
	"loads and merges commit only complete input"_test =
		test_loads_and_merges_commit_only_complete_input;
	"finite resource limits boundary and canonical output"_test =
		test_finite_resource_limits_boundary_and_canonical_output;
	"bounded reader stops a generated huge line"_test =
		test_bounded_reader_stops_a_generated_huge_line;
	"generated parse serialize parse invariants"_test =
		test_generated_parse_serialize_parse_invariants;
}

catch (const std::exception &error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
