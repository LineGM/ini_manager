#include "ini_manager/ini_manager.hpp"

auto main() -> int
{
  auto const result = name();

  return result == "ini_manager" ? 0 : 1;
}
