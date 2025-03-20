#include <iostream>
#include <sstream>

#include "ini_manager/ini_manager.hpp"

// NOLINTBEGIN(*-magic-numbers)
auto main() -> int
{
	std::cout
		<< "--- Example 3: Opening from a stream, reading, modifying, and writing ---"
		<< '\n';

	std::istringstream ini_stream3("[StreamSection]\nData = StreamData\nCounter = "
								   "5\n[ToRemove]\nItem = WillBeRemoved\n");
	auto config_result3 = ini::ini_manager::from_stream(ini_stream3);
	if (config_result3.has_value())
	{
		auto config3 = config_result3.value();
		if (auto data =
				config3.get_value(ini::section{"StreamSection"}, ini::key{"Data"}))
		{
			std::cout << "Data from stream: " << *data << '\n';
		}

		config3.set_value("StreamSection", "Counter", 10);
		config3.set_value("StreamSection", "NewValue", "StreamAdded");

		// Example of removing a section
		if (config3.remove_section(ini::section{"ToRemove"}))
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

		auto write_result3 = config3.write_file("stream_output.ini");
		if (write_result3.has_value())
		{
			std::cout << "Configuration from stream written to stream_output.ini" << '\n';
		}
		else
		{
			std::cerr << "Error writing to stream_output.ini: "
					  << write_result3.error().message() << '\n';
		}
	}
	else
	{
		std::cerr << "Error loading from stream." << '\n';
	}

	std::cout << '\n';
	return 0;
}

// NOLINTEND(*-magic-numbers)
