#include "header_a.hpp"
#include <ini_manager/ini_manager.hpp>
auto main() -> int
{
	try
	{
		ini::ini_manager config;
		constexpr int sample = 7;
		const auto result = config.set_value({"s"}, {"k"}, sample);
		auto read = config.get_value<int>({"s"}, {"k"});
		return result && read && header_a() == 1 && *read == sample ? 0 : 1;
	}
	catch (...)
	{
		return 1;
	}
}
