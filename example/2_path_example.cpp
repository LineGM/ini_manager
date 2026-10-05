#include "ini_manager/ini_manager.hpp"
#include "support.hpp"
#include <iostream>
auto main() -> int
{
	return example::run([] -> void {
		example::workspace const files;
		const auto path = files.directory / "config.ini";
		ini::ini_manager initial;
		example::checked(initial.set_value({"server"}, {"host"}, "localhost"));
		example::checked(initial.write_file(path));
		auto config = example::checked(ini::ini_manager::from_file(path));
		example::checked(config.set_value({"server"}, {"port"}, example::changed_port));
		example::checked(config.write_file()); // Safe replacement of associated path.
		example::checked(config.write_stream(std::cout));
	});
}
