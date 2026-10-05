#include "header_a.hpp"
#include <ini_manager/ini_manager.hpp>
auto header_a() -> int
{
	ini::ini_manager const config;
	return config.get_sections().empty() ? 1 : 0;
}
