#include "ini_manager/ini_manager.hpp"
#include <iostream>

auto main() -> int
{
	std::cout << "--- Example 4: Loading another file into an existing object ---"
			  << '\n';

	ini::ini_manager config4; // Create an empty object
	std::cout << "Empty config4 object created." << '\n';
	auto load_result4 =
		config4.load_file("existing.ini"); // Load the file created in example 2

	if (load_result4.has_value())
	{
		std::cout << "File existing.ini successfully loaded into config4." << '\n';
		if (auto number =
				config4.get_value<int>(ini::section{"Settings"}, ini::key{"Number"}))
		{
			std::cout << "Number from the loaded file: " << *number << '\n';
		}
	}
	else
	{
		std::cerr << "Error loading existing.ini into config4: "
				  << load_result4.error().message() << '\n';
	}

	std::cout << '\n';
	return 0;
}