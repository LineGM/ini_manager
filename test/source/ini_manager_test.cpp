#include "ini_manager/ini_manager.hpp"

#include <algorithm>
#include <boost/ut.hpp>

#include <sstream>
#include <string>
#include <vector>

// NOLINTBEGIN(*-magic-numbers)
auto main() -> int
{
	using boost::ut::expect;
	using boost::ut::suite;
	using boost::ut::bdd::given;
	using boost::ut::spec::describe;
	using boost::ut::spec::it;

	const suite ini_manager_tests = [] {
		describe("ini::trim") = [] {
			it("should remove leading and trailing whitespace") = [] {
				expect(ini::trim("  value  ") == "value");
				expect(ini::trim("\tvalue\t") == "value");
				expect(ini::trim("\rvalue\r") == "value");
				expect(ini::trim("\nvalue\n") == "value");
				expect(ini::trim(" \t\r\n value \t\r\n ") == "value");
			};

			it("should handle empty strings") = [] { expect(ini::trim("").empty()); };

			it("should handle strings with only whitespace") = [] {
				expect(ini::trim("  ").empty());
				expect(ini::trim("\t\t").empty());
				expect(ini::trim("\r\r").empty());
				expect(ini::trim("\n\n").empty());
				expect(ini::trim(" \t\r\n ").empty());
			};

			it("should handle strings with no whitespace") = [] {
				expect(ini::trim("value") == "value");
			};
		};

		describe("ini::ini::ini_manager") = [] {
			it("should be default constructible") = [] {
				const ini::ini_manager manager;
				expect(true);
			};

			describe("from_stream") = [] {
				it("should parse valid INI data from a stream") = [] {
					std::stringstream sstream;
					sstream << "[section1]\n";
					sstream << "key1 = value1\n";
					sstream << "key2 = value2\n";
					sstream << "\n";
					sstream << "[section2]\n";
					sstream << "key3 = value3\n";

					auto result = ini::ini_manager::from_stream(sstream);
					expect(result.has_value());
					const auto &manager = result.value();
					expect(manager.get_value(ini::section{"section1"},
											 ini::key{"key1"}) == "value1");
					expect(manager.get_value(ini::section{"section1"},
											 ini::key{"key2"}) == "value2");
					expect(manager.get_value(ini::section{"section2"},
											 ini::key{"key3"}) == "value3");
					expect(
						!manager.get_value(ini::section{"nonexistent"}, ini::key{"key"}));
				};

				it("should handle empty streams") = [] {
					std::stringstream sstream;
					auto result = ini::ini_manager::from_stream(sstream);
					expect(result.has_value());
				};

				it("should handle streams with only comments and whitespace") = [] {
					std::stringstream sstream;
					sstream << "; comment\n";
					sstream << "# another comment\n";
					sstream << "  \t\r\n";
					auto result = ini::ini_manager::from_stream(sstream);
					expect(result.has_value());
				};

				it("should handle streams with only sections") = [] {
					std::stringstream sstream;
					sstream << "[section1]\n";
					sstream << "[section2]\n";
					auto result = ini::ini_manager::from_stream(sstream);
					expect(result.has_value());
				};

				it("should handle streams with invalid syntax (no equals sign)") = [] {
					std::stringstream sstream;
					sstream << "[section]\n";
					sstream << "invalid_line\n";
					auto result = ini::ini_manager::from_stream(sstream);
					expect(result.has_value()); // Invalid lines are ignored
					const auto &manager = result.value();
					expect(!manager.get_value(ini::section{"section"},
											  ini::key{"invalid_line"}));
				};
			};

			describe("operator(non-const)") = [] {
				it("should allow modifying existing values") = [] {
					ini::ini_manager manager;
					manager["section"]["key"] = "initial_value";
					expect(manager["section"]["key"] == "initial_value");
					manager["section"]["key"] = "modified_value";
					expect(manager["section"]["key"] == "modified_value");
				};

				it("should create new sections and keys if they don't exist") = [] {
					ini::ini_manager manager;
					manager["new_section"]["new_key"] = "new_value";
					expect(manager.get_value(ini::section{"new_section"},
											 ini::key{"new_key"}) == "new_value");
				};
			};

			describe("operator(const)") = [] {
				it("should allow read-only access to existing values") = [] {
					ini::ini_manager manager;
					manager["section"]["key"] = "test_value";
					const auto &const_manager = manager;
					expect(const_manager["section"]["key"] == "test_value");
				};

				it("should return std::nullopt for non-existent sections or keys") = [] {
					const ini::ini_manager manager;
					const auto &const_manager = manager;
					expect(!const_manager["nonexistent_section"]["key"].has_value());
					expect(!const_manager["section"]["nonexistent_key"].has_value());
				};
			};

			describe("get_value (string)") = [] {
				given("an ini::ini_manager with a string value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "key", "string_value");
					it("should return the value as an optional string") = [&] {
						expect(manager.get_value(ini::section{"section"},
												 ini::key{"key"}) == "string_value");
					};
				};

				given("an ini::ini_manager with no such key") = [] {
					ini::ini_manager manager;
					it("should return an empty optional") = [&] {
						expect(!manager.get_value(ini::section{"section"},
												  ini::key{"nonexistent_key"}));
					};
				};

				given("an ini::ini_manager with no such section") = [] {
					ini::ini_manager manager;
					it("should return an empty optional") = [&] {
						expect(!manager.get_value(ini::section{"nonexistent_section"},
												  ini::key{"key"}));
					};
				};
			};

			describe("get_value (templated)") = [] {
				given("an ini::ini_manager with an integer value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "int_key", "123");
					it("should return the value as an optional integer") = [&] {
						expect(manager.get_value<int>(ini::section{"section"},
													  ini::key{"int_key"}) == 123);
					};
				};

				given("an ini::ini_manager with a double value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "first_double_key", "3.14");
					manager.set_value("section", "second_double_key", "33.1415926");
					it("should return the value as an optional double") = [&] {
						expect(manager.get_value<double>(ini::section{"section"},
														 ini::key{"first_double_key"}) ==
							   3.14);
						expect(manager.get_value<double>(ini::section{"section"},
														 ini::key{"second_double_key"}) ==
							   33.1415926);
					};
				};

				given("an ini::ini_manager with a boolean value (true)") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "bool_key_true", "true");
					manager.set_value("section", "bool_key_1", "1");
					manager.set_value("section", "bool_key_True", "True");
					it("should return the value as an optional boolean (true)") = [&] {
						expect(manager.get_value<bool>(ini::section{"section"},
													   ini::key{"bool_key_true"}) ==
							   true);
						expect(manager.get_value<bool>(ini::section{"section"},
													   ini::key{"bool_key_1"}) == true);
						expect(manager.get_value<bool>(ini::section{"section"},
													   ini::key{"bool_key_True"}) ==
							   true);
					};
				};

				given("an ini::ini_manager with a boolean value (false)") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "bool_key_false", "false");
					manager.set_value("section", "bool_key_0", "0");
					manager.set_value("section", "bool_key_False", "False");
					it("should return the value as an optional boolean (false)") = [&] {
						expect(manager.get_value<bool>(ini::section{"section"},
													   ini::key{"bool_key_false"}) ==
							   false);
						expect(manager.get_value<bool>(ini::section{"section"},
													   ini::key{"bool_key_0"}) == false);
						expect(manager.get_value<bool>(ini::section{"section"},
													   ini::key{"bool_key_False"}) ==
							   false);
					};
				};

				given("an ini::ini_manager with an invalid boolean value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "bool_key_invalid", "invalid");
					it("should return an empty optional for invalid boolean") = [&] {
						expect(!manager.get_value<bool>(ini::section{"section"},
														ini::key{"bool_key_invalid"}));
					};
				};

				given("an ini::ini_manager with a non-numeric string for numeric types") =
					[] {
						ini::ini_manager manager;
						manager.set_value("section", "invalid_int", "abc");
						manager.set_value("section", "invalid_double", "xyz");
						it("should return an empty optional for invalid numeric "
						   "conversions") = [&] {
							expect(!manager.get_value<int>(ini::section{"section"},
														   ini::key{"invalid_int"}));
							expect(!manager.get_value<double>(
								ini::section{"section"}, ini::key{"invalid_double"}));
						};
					};
			};

			describe("get_value_or_default (string)") = [] {
				given("an ini::ini_manager with a value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "key", "value");
					it("should return the value") = [&] {
						expect(manager.get_value_or_default<std::string>(
								   ini::section{"section"}, ini::key{"key"}, "default") ==
							   "value");
					};
				};

				given("an ini::ini_manager without the value") = [] {
					ini::ini_manager manager;
					it("should return the default value") = [&] {
						expect(manager.get_value_or_default<std::string>(
								   ini::section{"section"}, ini::key{"nonexistent_key"},
								   "default") == "default");
					};
				};
			};

			describe("get_value_or_default (templated)") = [] {
				given("an ini::ini_manager with an integer value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "int_key", "123");
					it("should return the value") = [&] {
						expect(manager.get_value_or_default(ini::section{"section"},
															ini::key{"int_key"},
															456) == 123);
					};
				};

				given("an ini::ini_manager without the value") = [] {
					ini::ini_manager manager;
					it("should return the default value") = [&] {
						expect(manager.get_value_or_default(ini::section{"section"},
															ini::key{"nonexistent_key"},
															456) == 456);
					};
				};

				given("an ini::ini_manager with an invalid value for the type") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "invalid_int", "abc");
					it("should return the default value") = [&] {
						expect(manager.get_value_or_default(ini::section{"section"},
															ini::key{"invalid_int"},
															456) == 456);
					};
				};
			};

			describe("set_value") = [] {
				it("should set a string value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "key", "value");
					expect(manager.get_value(ini::section{"section"}, ini::key{"key"}) ==
						   "value");
				};

				it("should set an integer value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "int_key", 123);
					expect(manager.get_value(ini::section{"section"},
											 ini::key{"int_key"}) == "123");
				};

				it("should set a double value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "double_key", 3.14);
					expect(manager.get_value(ini::section{"section"},
											 ini::key{"double_key"}) == "3.14");
				};

				it("should create the section if it doesn't exist") = [] {
					ini::ini_manager manager;
					manager.set_value("new_section", "key", "value");
					expect(manager.get_value(ini::section{"new_section"},
											 ini::key{"key"}) == "value");
				};

				it("should update the value if the key already exists") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "key", "old_value");
					manager.set_value("section", "key", "new_value");
					expect(manager.get_value(ini::section{"section"}, ini::key{"key"}) ==
						   "new_value");
				};
			};

			describe("set_section") = [] {
				it("should create a new section if it doesn't exist") = [] {
					ini::ini_manager manager;
					manager.set_section("new_section");
					// Check if section exists
					expect(manager["new_section"]["some_key"].empty());
				};

				it("should not overwrite an existing section") = [] {
					ini::ini_manager manager;
					manager.set_value("existing_section", "key", "value");
					manager.set_section("existing_section");
					expect(manager.get_value(ini::section{"existing_section"},
											 ini::key{"key"}) == "value");
				};
			};

			describe("remove_value") = [] {
				given("an ini::ini_manager with a value") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "key", "value");
					it("should remove the value and return true") = [&] {
						expect(manager.remove_value(ini::section{"section"},
													ini::key{"key"}));
						expect(
							!manager.get_value(ini::section{"section"}, ini::key{"key"}));
					};
				};

				given("an ini::ini_manager without the value") = [] {
					ini::ini_manager manager;
					it("should return false") = [&] {
						expect(!manager.remove_value(ini::section{"section"},
													 ini::key{"nonexistent_key"}));
					};
				};

				given("an ini::ini_manager without the section") = [] {
					ini::ini_manager manager;
					it("should return false") = [&] {
						expect(!manager.remove_value(ini::section{"nonexistent_section"},
													 ini::key{"key"}));
					};
				};
			};

			describe("remove_section") = [] {
				given("an ini::ini_manager with a section") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "key", "value");
					it("should remove the section and return true") = [&] {
						expect(manager.remove_section(ini::section{"section"}));
						expect(manager["section"]["key"].empty());
					};
				};

				given("an ini::ini_manager without the section") = [] {
					ini::ini_manager manager;
					it("should return false") = [&] {
						expect(
							!manager.remove_section(ini::section{"nonexistent_section"}));
					};
				};
			};

			// Helper function to compare vectors regardless of order
			auto vectors_contain_same_elements =
				[](std::vector<std::string> vec1, std::vector<std::string> vec2) -> bool {
				std::ranges::sort(vec1);
				std::ranges::sort(vec2);
				return vec1 == vec2;
			};

			describe("get_sections") = [&vectors_contain_same_elements] {
				it("should return empty vector for empty manager") = [] {
					const ini::ini_manager manager;
					expect(manager.get_sections().empty());
				};

				it("should return vector with one section name") = [] {
					ini::ini_manager manager;
					manager.set_section("General");
					auto sections = manager.get_sections();
					expect(sections.size() == 1U);
					expect(sections[0] == "General");
				};

				it("should return vector with one section name (added via set_value)") =
					[] {
						ini::ini_manager manager;
						manager.set_value("General", "key", "value");
						auto sections = manager.get_sections();
						expect(sections.size() == 1U);
						expect(sections[0] == "General");
					};

				it("should return vector with multiple section names") = [&] {
					ini::ini_manager manager;
					manager.set_section("Database");
					manager.set_value("User", "id", "123");
					manager.set_section("General");

					auto sections = manager.get_sections();
					const std::vector<std::string> expected = {"Database", "User",
															   "General"};

					expect(sections.size() == expected.size());
					expect(vectors_contain_same_elements(sections, expected));
				};

				it("should reflect added sections dynamically") = [&] {
					ini::ini_manager manager;
					expect(manager.get_sections().empty());
					manager.set_section("First");
					expect(
						vectors_contain_same_elements(manager.get_sections(), {"First"}));
					manager.set_section("Second");
					expect(vectors_contain_same_elements(manager.get_sections(),
														 {"First", "Second"}));
				};

				it("should reflect removed sections") = [&] {
					ini::ini_manager manager;
					manager.set_section("Section1");
					manager.set_section("SectionToRemove");
					manager.set_section("Section3");
					expect(vectors_contain_same_elements(
						manager.get_sections(),
						{"Section1", "SectionToRemove", "Section3"}));

					manager.remove_section(ini::section{"SectionToRemove"});
					expect(vectors_contain_same_elements(manager.get_sections(),
														 {"Section1", "Section3"}));
				};

				it("should handle section names with spaces (if supported by set)") =
					[&] {
						ini::ini_manager manager;
						manager.set_section(
							" User Settings "); // Assumes set_section stores it as is
						// But parsing trims section names, so let's test parsed names
						std::stringstream sstream;
						sstream << "[ User Settings ]\nkey=value\n[Another Section]\n";
						auto result = ini::ini_manager::from_stream(sstream);
						expect(result.has_value());
						auto sections = result.value().get_sections();
						expect(vectors_contain_same_elements(
							sections, {"User Settings",
									   "Another Section"})); // Expect trimmed names
					};

				it("should handle empty section name (if supported by parsing)") = [&] {
					const ini::ini_manager manager;
					std::stringstream sstream;
					sstream << "[]\nkey=value\n"; // Parsed as empty section name ""
					auto result = ini::ini_manager::from_stream(sstream);
					expect(result.has_value());
					auto sections = result.value().get_sections();
					expect(vectors_contain_same_elements(sections, {""}));
				};
			};

			describe("get_keys") = [&vectors_contain_same_elements] {
				given("a manager with specific sections and keys") =
					[&vectors_contain_same_elements] {
						ini::ini_manager manager;
						manager.set_section("EmptySection");
						manager.set_value("General", "Host", "localhost");
						manager.set_value("General", "Port", "8080");
						manager.set_value("Database", "Type", "sqlite");
						manager.set_value("Database", "File", "data.db");
						manager.set_value("Database", "Timeout", "5000");

						it("should return vector with multiple keys for a section") =
							[&] {
								auto keys = manager.get_keys(ini::section{"General"});
								std::vector<std::string> expected = {"Host", "Port"};
								expect(keys.size() == expected.size());
								expect(vectors_contain_same_elements(keys, expected));

								keys = manager.get_keys(ini::section{"Database"});
								expected = {"Type", "File", "Timeout"};
								expect(keys.size() == expected.size());
								expect(vectors_contain_same_elements(keys, expected));
							};

						it("should return empty vector for a non-existent section") =
							[&] {
								expect(manager.get_keys(ini::section{"NonExistent"})
										   .empty());
							};
					};
			};

			describe("load_stream") = [] {
				it("should clear existing data and load from a stream") = [] {
					ini::ini_manager manager;
					manager.set_value("existing_section", "existing_key",
									  "existing_value");
					std::stringstream sstream;
					sstream << "[new_section]\n";
					sstream << "new_key = new_value\n";
					auto result = manager.load_stream(sstream);
					expect(result.has_value());
					expect(!manager.get_value(ini::section{"existing_section"},
											  ini::key{"existing_key"}));
					expect(manager.get_value(ini::section{"new_section"},
											 ini::key{"new_key"}) == "new_value");
				};
			};

			describe("add_from_stream") = [] {
				it("should add data from a stream to existing data") = [] {
					ini::ini_manager manager;
					manager.set_value("existing_section", "existing_key",
									  "existing_value");
					std::stringstream sstream;
					sstream << "[new_section]\n";
					sstream << "new_key = new_value\n";
					auto result = manager.add_from_stream(sstream);
					expect(result.has_value());
					expect(manager.get_value(ini::section{"existing_section"},
											 ini::key{"existing_key"}) ==
						   "existing_value");
					expect(manager.get_value(ini::section{"new_section"},
											 ini::key{"new_key"}) == "new_value");
				};

				it("should update existing values from the stream") = [] {
					ini::ini_manager manager;
					manager.set_value("section", "key", "old_value");
					std::stringstream sstream;
					sstream << "[section]\n";
					sstream << "key = new_value\n";
					auto result = manager.add_from_stream(sstream);
					expect(result.has_value());
					expect(manager.get_value(ini::section{"section"}, ini::key{"key"}) ==
						   "new_value");
				};
			};

			describe("operator<< (ostream)") = [] {
				it("should write the INI data to the output stream") = [] {
					ini::ini_manager manager;
					manager.set_value("section1", "key1", "value1");
					manager.set_value("section2", "key2", "value2");
					std::stringstream sstream;
					sstream << manager;
					const std::string expected =
						"[section1]\nkey1 = value1\n\n[section2]\nkey2 = value2\n\n";
					expect(sstream.str() == expected);
				};
			};

			describe("operator>> (istream)") = [] {
				it("should parse INI data from the input stream") = [] {
					ini::ini_manager manager;
					std::stringstream sstream;
					sstream << "[section1]\nkey1 = value1\n[section2]\nkey2 = value2\n";
					sstream >> manager;
					expect(manager.get_value(ini::section{"section1"},
											 ini::key{"key1"}) == "value1");
					expect(manager.get_value(ini::section{"section2"},
											 ini::key{"key2"}) == "value2");
				};

				it("should set failbit on the stream if parsing fails (simulated by "
				   "empty stream after some data)") = [] {
					ini::ini_manager manager;
					std::stringstream sstream;
					sstream
						<< "[section]\nkey = value\n[another_section]"; // Incomplete line
					sstream >> manager;
					expect(sstream.fail());
				};
			};
		};
	};
}
// NOLINTEND(*-magic-numbers)
