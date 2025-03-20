#include "ini_manager/ini_manager.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

auto main() -> int
{
	ini::ini_manager config;

	std::cout << "--- Example 7: Adding data from a file ---" << '\n';

	// Create the first temporary INI file
	const std::string temp_file_path1 = "temp_add_file1.ini";
	std::ofstream temp_ini_file1(temp_file_path1);
	if (temp_ini_file1.is_open())
	{
		temp_ini_file1 << "[SectionC]\n";
		temp_ini_file1 << "Key4 = Value4\n";
		temp_ini_file1 << "Key5 = Value5\n";
		temp_ini_file1.close();
		std::cout << "Created temporary INI file: " << temp_file_path1 << '\n';
	}
	else
	{
		std::cerr << "Error creating temporary INI file: " << temp_file_path1 << '\n';
		return 1;
	}

	auto result = config.add_from_file(temp_file_path1);
	if (result.has_value())
	{
		if (auto value = config.get_value(ini::section{"SectionC"}, ini::key{"Key4"}))
		{
			std::cout << "SectionC::Key4: " << *value << '\n';
		}
		if (auto value = config.get_value(ini::section{"SectionC"}, ini::key{"Key5"}))
		{
			std::cout << "SectionC::Key5: " << *value << '\n';
		}
	}
	else
	{
		std::cerr << "Error in add_from_file (" << temp_file_path1
				  << "): " << result.error().message() << '\n';
	}

	// Create the second temporary INI file to add more data
	const std::string temp_file_path2 = "temp_add_file2.ini";
	std::ofstream temp_ini_file2(temp_file_path2);
	if (temp_ini_file2.is_open())
	{
		temp_ini_file2 << "[SectionD]\n";
		temp_ini_file2 << "Key6 = Value6\n";
		temp_ini_file2 << "[SectionC]\n";		   // Add to an existing section
		temp_ini_file2 << "Key5 = UpdatedValue\n"; // Update an existing key
		temp_ini_file2.close();
		std::cout << "Created temporary INI file: " << temp_file_path2 << '\n';
	}
	else
	{
		std::cerr << "Error creating temporary INI file: " << temp_file_path2 << '\n';
		std::filesystem::remove(temp_file_path1);
		return 1;
	}

	result = config.add_from_file(temp_file_path2);
	if (result.has_value())
	{
		if (auto value = config.get_value(ini::section{"SectionD"}, ini::key{"Key6"}))
		{
			std::cout << "SectionD::Key6: " << *value << '\n';
		}
		if (auto value = config.get_value(ini::section{"SectionC"}, ini::key{"Key4"}))
		{
			std::cout << "SectionC::Key4 (after second add): " << *value << '\n';
		}
		if (auto value = config.get_value(ini::section{"SectionC"}, ini::key{"Key5"}))
		{
			std::cout << "SectionC::Key5 (after second add): " << *value << '\n';
		}
	}
	else
	{
		std::cerr << "Error in add_from_file (" << temp_file_path2
				  << "): " << result.error().message() << '\n';
	}
	std::cout << '\n';

	// Clean up temporary files
	std::filesystem::remove(temp_file_path1);
	std::filesystem::remove(temp_file_path2);

	return 0;
}