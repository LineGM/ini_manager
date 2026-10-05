#include "ini_manager/ini_manager.hpp"
#include "support.hpp"
#include <iostream>
auto main() -> int
{
	return example::run([] -> void {
		example::workspace const files;
		const auto path = files.directory / "overlay.ini";
		ini::ini_manager overlay;
		example::checked(overlay.set_value({"server"}, {"port"}, example::changed_port));
		example::checked(overlay.write_file(path));
		ini::ini_manager config;
		example::checked(config.set_value({"server"}, {"host"}, "localhost"));
		example::checked(config.add_from_file(path));
		example::checked(config.write_stream(std::cout));
	});
}
