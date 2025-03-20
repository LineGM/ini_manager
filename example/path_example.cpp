#include "ini_manager/ini_manager.hpp"
#include <fstream>
#include <iostream>

// NOLINTBEGIN(*-magic-numbers)
auto main() -> int
{
	std::cout << "--- Example 2: Opening an existing ini file, reading, modifying, and "
				 "writing to it ---"
			  << '\n';

	// Create the file for this example first
	std::ofstream existing_ini("existing.ini");
	if (existing_ini.is_open())
	{
		existing_ini << "[Settings]\n";
		existing_ini << "Option1 = OldValue\n";
		existing_ini << "Number = 100\n";
		existing_ini << "[ToDelete]\n";
		existing_ini << "KeyToDelete = SomeValue\n";
		existing_ini.close();
	}

	auto config_result2 = ini::ini_manager::from_file("existing.ini");
	if (config_result2.has_value())
	{
		auto config2 = config_result2.value();
		if (auto option1 =
				config2.get_value(ini::section{"Settings"}, ini::key{"Option1"}))
		{
			std::cout << "Option1 before modification: " << *option1 << '\n';
		}

		config2.set_value("Settings", "Option1", "NewValue");
		config2.set_value("Settings", "NewOption", "AddedOption");
		config2.set_value("Settings", "Number", 200);

		// Example of removing an entry
		if (config2.remove_value(ini::section{"ToDelete"}, ini::key{"KeyToDelete"}))
		{
			std::cout << "Entry ToDelete::KeyToDelete successfully removed." << '\n';
		}
		else
		{
			std::cout << "Entry ToDelete::KeyToDelete not found." << '\n';
		}

		// Write back to the same file
		auto write_result2 = config2.write_file("existing.ini");
		if (write_result2.has_value())
		{
			std::cout << "File existing.ini successfully updated." << '\n';
		}
		else
		{
			std::cerr << "Error writing to existing.ini: "
					  << write_result2.error().message() << '\n';
		}
	}
	else
	{
		std::cerr << "Error opening existing.ini: " << config_result2.error().message()
				  << '\n';
	}

	std::cout << '\n';
	return 0;
}
// NOLINTEND(*-magic-numbers)