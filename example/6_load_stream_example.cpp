#include "ini_manager/ini_manager.hpp"
#include <iostream>
#include <sstream>

auto main() -> int
{
	ini::ini_manager config;

	// Добавим немного данных, чтобы показать, что load_stream их очистит
	std::istringstream initial_ini_stream(
		"[InitialSection]\nInitialKey = InitialValue\n");
	auto result = config.add_from_stream(initial_ini_stream);
	if (result.has_value())
	{
		if (config.get_value(ini::section{"InitialSection"}, ini::key{"InitialKey"}))
		{
			std::cout << "InitialSection::InitialKey exists." << '\n';
		}
	}

	std::cout << "--- Example 6: Loading data from a stream ---" << '\n';
	std::istringstream to_load_ini_stream("[SectionC]\nKey4 = Value4\n");
	result = config.load_stream(to_load_ini_stream);
	if (result.has_value())
	{
		if (auto value = config.get_value(ini::section{"SectionC"}, ini::key{"Key4"}))
		{
			std::cout << "SectionC::Key4: " << *value << '\n';
		}

		// Check if data from previous add_from_stream was removed
		if (config.get_value(ini::section{"InitialSection"}, ini::key{"InitialKey"})
				.has_value())
		{
			std::cout << "SectionA::Key1 still exists (load_stream error)." << '\n';
		}
		else
		{
			std::cout << "Previous data from add_from_stream was removed (as expected)"
					  << '\n';
		}
	}
	else
	{
		std::cerr << "load_stream error (to_load_ini_stream): "
				  << result.error().message() << '\n';
	}
	std::cout << '\n';

	return 0;
}