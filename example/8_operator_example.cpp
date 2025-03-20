#include "ini_manager/ini_manager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

auto main() -> int
{
	ini::ini_manager config;

	std::cout << "--- Example 8: Using operator>> from string stream ---" << '\n';
	std::istringstream ini_stream("[SectionA]\nKey1 = Value1\nKey2 = Value2\n");
	ini_stream >> config;

	if (ini_stream.bad() || (ini_stream.fail() && !ini_stream.eof()))
	{
		std::cerr << "Error loading config from stream." << '\n';
	}
	else
	{
		std::cout << config << '\n';
	}

	ini::ini_manager config2;
	const std::string temp_file_path = "temp_config.ini";
	{
		std::ofstream temp_file(temp_file_path);
		temp_file << "[SectionB]\nKey3 = Value3\nKey4 = Value4\n";
	}

	std::cout << "\n--- Example using operator>> from file ---" << '\n';
	std::ifstream file_stream(temp_file_path);
	if (file_stream.is_open())
	{
		file_stream >> config2;
		if (file_stream.bad() || (file_stream.fail() && !file_stream.eof()))
		{
			std::cerr << "Error loading config2 from file." << '\n';
		}
		else
		{
			std::cout << config2 << '\n';
		}
		file_stream.close();
	}
	else
	{
		std::cerr << "Error opening file: " << temp_file_path << '\n';
	}

	std::filesystem::remove(temp_file_path);

	return 0;
}