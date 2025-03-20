#include "ini_manager/ini_manager.hpp"
#include <iostream>

// NOLINTBEGIN(*-magic-numbers)
auto main() -> int
{
	std::cout << "--- Example 1: Creating an empty object, populating, and writing to a "
				 "file ---"
			  << '\n';

	ini::ini_manager config1;

	config1.set_section("Section1");
	config1.set_value("Section1", "Key1", "Value1");
	config1.set_value("Section1", "KeyInt", 42);

	auto write_result1 = config1.write_file("empty_config.ini");
	if (write_result1.has_value())
	{
		std::cout << "Configuration successfully written to empty_config.ini" << '\n';
	}
	else
	{
		std::cerr << "Error writing to empty_config.ini: "
				  << write_result1.error().message() << '\n';
	}

	std::cout << '\n';
	return 0;
}
// NOLINTEND(*-magic-numbers)