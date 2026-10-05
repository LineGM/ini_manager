#include "ini_manager/ini_manager.hpp"
#include "support.hpp"
#include <iostream>
#include <sstream>
auto main() -> int
{
	return example::run([] -> void {
		ini::ini_manager config;
		example::checked(config.set_value({"server"}, {"host"}, "localhost"));
		example::checked(config.set_value({"server"}, {"port"}, example::initial_port));
		std::istringstream overlay("[server]\nport=9000\n[empty]\n");
		example::checked(config.add_from_stream(overlay));
		example::checked(config.write_stream(std::cout));
	});
}
