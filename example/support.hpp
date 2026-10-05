#ifndef INI_MANAGER_EXAMPLE_SUPPORT_HPP
#define INI_MANAGER_EXAMPLE_SUPPORT_HPP
#include <ini_manager/ini_manager.hpp>
#include <iostream>
#include <stdexcept>
namespace example
{
inline constexpr unsigned initial_port = 8080;
inline constexpr unsigned changed_port = 9000;
template <class T> auto checked(ini::result<T> value) -> T
{
	if (!value)
	{
		throw std::runtime_error(value.error().message());
	}
	if constexpr (!std::same_as<T, void>)
	{
		return std::move(*value);
	}
}
template <class Function> auto run(Function function) -> int
{
	try
	{
		function();
		return 0;
	}
	catch (const std::exception &exception)
	{
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
struct workspace
{
	std::filesystem::path directory;
	workspace()
	{
		constexpr unsigned max_attempts = 10000;
		for (unsigned i = 0; i < max_attempts; ++i)
		{
			directory = std::filesystem::temp_directory_path() /
						("ini-manager-example-" + std::to_string(i));
			std::error_code code;
			if (std::filesystem::create_directory(directory, code))
			{
				return;
			}
		}
		throw std::runtime_error("Cannot create example directory");
	}
	workspace(const workspace &) = delete;
	workspace(workspace &&) = delete;
	auto operator=(const workspace &) -> workspace & = delete;
	auto operator=(workspace &&) -> workspace & = delete;
	~workspace()
	{
		std::error_code code;
		std::filesystem::remove_all(directory, code);
	}
};
} // namespace example
#endif
