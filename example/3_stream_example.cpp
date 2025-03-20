#include "ini_manager/ini_manager.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>

// NOLINTBEGIN(*-magic-numbers)
auto main() -> int
{
	std::cout
		<< "--- Example 3: Opening from a stream, reading, modifying, and writing ---"
		<< '\n';

	std::istringstream example_ini_stream("[StreamSection]\nData = StreamData\nCounter = "
										  "5\n[ToRemove]\nItem = WillBeRemoved\n");
	auto config_result = ini::ini_manager::from_stream(example_ini_stream);
	if (config_result.has_value())
	{
		auto config = config_result.value();
		if (auto data = config.get_value(ini::section{"StreamSection"}, ini::key{"Data"}))
		{
			std::cout << "Data from stream: " << *data << '\n';
		}

		config.set_value("StreamSection", "Counter", 10);
		config.set_value("StreamSection", "NewValue", "StreamAdded");

		// Example of removing a section
		if (config.remove_section(ini::section{"ToRemove"}))
		{
			std::cout
				<< "Section ToRemove successfully removed from the stream configuration."
				<< '\n';
		}
		else
		{
			std::cout << "Section ToRemove not found in the stream configuration."
					  << '\n';
		}

		auto write_result = config.write_file("example3.ini");
		if (write_result.has_value())
		{
			std::cout << "Configuration from stream written to example3.ini" << '\n';
		}
		else
		{
			std::cerr << "Error writing to example3.ini: "
					  << write_result.error().message() << '\n';
		}
	}
	else
	{
		std::cerr << "Error loading from stream." << '\n';
	}

	std::filesystem::remove("example3.ini");

	std::cout << '\n';
	return 0;
}

// NOLINTEND(*-magic-numbers)
