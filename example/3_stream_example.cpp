#include "ini_manager/ini_manager.hpp"
#include "support.hpp"
#include <iostream>
#include <sstream>
auto main() -> int
{
	return example::run([] -> void {
		std::istringstream input("[server]\nport=8080\nenabled=TRUE\n");
		const auto config = example::checked(ini::ini_manager::from_stream(input));
		std::cout << example::checked(config.get_value<unsigned>({"server"}, {"port"}))
				  << '\n';
		std::cout << example::checked(config.get_value<bool>({"server"}, {"enabled"}))
				  << '\n';
		std::cout << example::checked(
						 config.get_value_or_default({"server"}, {"host"}, "localhost"))
				  << '\n';
		auto invalid = config.get_value<unsigned>({"server"}, {"enabled"});
		if (!invalid)
		{
			std::cout << invalid.error().message() << '\n';
		}
	});
}
