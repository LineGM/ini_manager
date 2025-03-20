#include "ini_manager/ini_manager.hpp"

#include <filesystem>
#include <iostream>

// NOLINTBEGIN(*-magic-numbers)
auto main() -> int
{
	std::cout << "--- Example 1: Creating an empty object, populating, and writing to a "
				 "file ---"
			  << '\n';

	ini::ini_manager config;

	config.set_section("Section1");
	config.set_value("Section1", "Key1", "Value1");
	config.set_value("Section1", "KeyInt", 42);

	auto write_result = config.write_file("example1.ini");
	if (write_result.has_value())
	{
		std::cout << "Configuration successfully written to example1.ini" << '\n';
	}
	else
	{
		std::cerr << "Error writing to example1.ini: " << write_result.error().message()
				  << '\n';
	}

	std::filesystem::remove("example1.ini");

	std::cout << '\n';
	return 0;
}
// NOLINTEND(*-magic-numbers)