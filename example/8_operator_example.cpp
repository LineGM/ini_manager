#include "ini_manager/ini_manager.hpp"
#include "support.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
auto main() -> int
{
	return example::run([] -> void {
		ini::ini_manager config;
		std::istringstream input("[server]\nport=8080\n[empty]");
		if (!(input >> config))
		{
			throw std::runtime_error("INI extraction failed");
		}
		if (!(std::cout << config))
		{
			throw std::runtime_error("INI insertion failed");
		}
		// Use load_stream/write_stream when structured diagnostics are needed.
	});
}
