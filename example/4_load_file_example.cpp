#include "ini_manager/ini_manager.hpp"
#include "support.hpp"
#include <iostream>
auto main() -> int
{
	return example::run([] -> void {
		example::workspace const files;
		const auto path = files.directory / "config.ini";
		ini::ini_manager saved;
		example::checked(saved.set_value({"server"}, {"port"}, example::initial_port));
		example::checked(saved.write_file(path));
		ini::ini_manager config;
		example::checked(config.load_file(path));
		if (auto failed = config.load_file(files.directory / "missing.ini"); !failed)
		{
			std::cout << failed.error().message() << '\n';
		}
		// The successful configuration and its associated path survive the failure.
		example::checked(config.write_stream(std::cout));
		example::checked(config.write_file());
	});
}
