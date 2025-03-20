#include "ini_manager/ini_manager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

// NOLINTBEGIN(*-magic-numbers)
auto main() -> int
{
	std::cout << "--- Example 2: Opening an existing ini file, reading, modifying, and "
				 "writing to it ---"
			  << '\n';

	// Create the file for this example first
	std::ofstream example_ini("example2.ini");
	if (example_ini.is_open())
	{
		example_ini << "[Settings]\n";
		example_ini << "Option1 = OldValue\n";
		example_ini << "Number = 100\n";
		example_ini << "[ToDelete]\n";
		example_ini << "KeyToDelete = SomeValue\n";
		example_ini.close();
	}

	auto config_result = ini::ini_manager::from_file("example2.ini");
	if (config_result.has_value())
	{
		auto config = config_result.value();
		if (auto option = config.get_value(ini::section{"Settings"}, ini::key{"Option1"}))
		{
			std::cout << "Option1 before modification: " << *option << '\n';
		}

		config.set_value("Settings", "Option1", "NewValue");
		config.set_value("Settings", "NewOption", "AddedOption");
		config.set_value("Settings", "Number", 200);

		// Example of removing an entry
		if (config.remove_value(ini::section{"ToDelete"}, ini::key{"KeyToDelete"}))
		{
			std::cout << "Entry ToDelete::KeyToDelete successfully removed." << '\n';
		}
		else
		{
			std::cout << "Entry ToDelete::KeyToDelete not found." << '\n';
		}

		// Write back to the same file
		auto write_result = config.write_file("example2.ini");
		if (write_result.has_value())
		{
			std::cout << "File example2.ini successfully updated." << '\n';
		}
		else
		{
			std::cerr << "Error writing to example2.ini: "
					  << write_result.error().message() << '\n';
		}
	}
	else
	{
		std::cerr << "Error opening example2.ini: " << config_result.error().message()
				  << '\n';
	}

	std::filesystem::remove("example2.ini");

	std::cout << '\n';
	return 0;
}
// NOLINTEND(*-magic-numbers)