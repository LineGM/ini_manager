#include "ini_manager/ini_manager.hpp"
#include "support.hpp"
#include <iostream>
#include <sstream>
auto main() -> int
{
	return example::run([] -> void {
		ini::ini_manager config;
		example::checked(config.set_value({"old"}, {"x"}, "old"));
		std::istringstream broken("[new]\nx=value\n[broken");
		if (auto error = config.load_stream(broken); !error)
		{
			std::cout << error.error().message() << '\n';
		}
		example::checked(config.write_stream(std::cout));
		std::istringstream replacement("global=yes\n[new] ; comment\nx=value");
		example::checked(config.load_stream(replacement));
		example::checked(config.write_stream(std::cout));
	});
}
