#include "ini_manager/ini_manager.hpp"
#include <iostream>
#include <sstream>

auto main() -> int
{
	ini::ini_manager config;

	std::cout << "--- Example 5: Adding data from a stream ---" << '\n';
	std::istringstream example_ini_stream("[SectionA]\nKey1 = Value1\nKey2 = Value2\n");

	auto result = config.add_from_stream(example_ini_stream);
	if (result.has_value())
	{
		if (auto value = config.get_value(ini::section{"SectionA"}, ini::key{"Key1"}))
		{
			std::cout << "SectionA::Key1: " << *value << '\n';
		}
		if (auto value = config.get_value(ini::section{"SectionA"}, ini::key{"Key2"}))
		{
			std::cout << "SectionA::Key2: " << *value << '\n';
		}
	}
	else
	{
		std::cerr << "Error in add_from_stream (example_ini_stream): "
				  << result.error().message() << '\n';
	}

	std::istringstream to_add_ini_stream("[SectionB]\nKey3 = Value3\n");

	result = config.add_from_stream(to_add_ini_stream);
	if (result.has_value())
	{
		if (auto value = config.get_value(ini::section{"SectionB"}, ini::key{"Key3"}))
		{
			std::cout << "SectionB::Key3: " << *value << '\n';
		}
		if (auto value = config.get_value(ini::section{"SectionA"}, ini::key{"Key1"}))
		{
			std::cout << "SectionA::Key1 (after second add): " << *value << '\n';
		}
	}
	else
	{
		std::cerr << "Error in add_from_stream (to_add_ini_stream): "
				  << result.error().message() << '\n';
	}
	std::cout << '\n';

	return 0;
}