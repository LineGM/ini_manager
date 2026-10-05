#include "ini_manager/ini_manager.hpp"
#include "support.hpp"
#include <iostream>
auto main() -> int
{
	return example::run([] -> void {
		ini::ini_manager config;
		example::checked(config.set_value({"server"}, {"port"}, example::initial_port));
		example::checked(config.set_section({"empty"}));
		auto backup = config;
		example::checked(backup.set_value({"server"}, {"port"}, example::changed_port));
		std::cout << "Original port: "
				  << example::checked(config.get_value<unsigned>({"server"}, {"port"}))
				  << '\n';
		example::checked(config.write_stream(std::cout));
	});
}
