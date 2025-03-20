#include "ini_manager/ini_manager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

auto main() -> int
{
	std::cout << "--- Example 4: Loading another file into an existing object ---"
			  << '\n';

	// Create the example4.ini
	std::ofstream example_ini("example4.ini");
	if (example_ini.is_open())
	{
		example_ini << "[Settings]\n";
		example_ini << "Option1 = OldValue\n";
		example_ini << "Number = 100\n";
		example_ini << "[ToDelete]\n";
		example_ini << "KeyToDelete = SomeValue\n";
		example_ini.close();
		std::cout << "Created example4.ini for this example." << '\n';
	}
	else
	{
		std::cerr << "Error creating example4.ini for this example." << '\n';
		return 1; // Indicate an error
	}

	ini::ini_manager config; // Create an empty object
	std::cout << "Empty config object created." << '\n';
	auto load_result = config.load_file("example4.ini"); // Load the file we just created

	if (load_result.has_value())
	{
		std::cout << "File example4.ini successfully loaded into config." << '\n';
		if (auto number =
				config.get_value<int>(ini::section{"Settings"}, ini::key{"Number"}))
		{
			std::cout << "Number from the loaded file: " << *number << '\n';
		}
	}
	else
	{
		std::cerr << "Error loading example4.ini into config: "
				  << load_result.error().message() << '\n';
	}

	std::filesystem::remove("example4.ini");

	std::cout << '\n';
	return 0;
}