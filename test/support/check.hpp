#ifndef INI_MANAGER_TEST_CHECK_HPP
#define INI_MANAGER_TEST_CHECK_HPP
#include <atomic>
#include <boost/ut.hpp>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <ini_manager/ini_manager.hpp>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace test
{
inline void require(bool value)
{
	if (!value)
	{
		throw std::runtime_error("test precondition failed");
	}
}
template <class T> auto must(ini::result<T> value) -> T
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
inline auto parse(std::string_view text, ini::parse_options options = {})
	-> ini::ini_manager
{
	std::istringstream input{std::string(text)};
	return must(ini::ini_manager::from_stream(input, options));
}
inline auto serialize(const ini::ini_manager &value) -> std::string
{
	std::ostringstream out;
	must(value.write_stream(out));
	return out.str();
}
inline auto same_data(const ini::ini_manager &left, const ini::ini_manager &right) -> bool
{
	if (left.get_sections() != right.get_sections())
	{
		return false;
	}
	for (const auto &section : left.get_sections())
	{
		if (left.get_keys({section}) != right.get_keys({section}))
		{
			return false;
		}
		for (const auto &key : left.get_keys({section}))
		{
			if (must(left.get_value({section}, {key})) !=
				must(right.get_value({section}, {key})))
			{
				return false;
			}
		}
	}
	return true;
}
struct temp_directory
{
	std::filesystem::path path;
	temp_directory()
	{
		static std::atomic<unsigned> count{0};
		const auto base = std::filesystem::temp_directory_path();
		constexpr unsigned max_attempts = 1000;
		for (unsigned i = 0; i < max_attempts; ++i)
		{
			path = base / ("ini-manager-test-" + std::to_string(count.fetch_add(1)));
			std::error_code code;
			if (std::filesystem::create_directory(path, code))
			{
				return;
			}
		}
		throw std::runtime_error("cannot create test directory");
	}
	temp_directory(const temp_directory &) = delete;
	temp_directory(temp_directory &&) = delete;
	auto operator=(const temp_directory &) -> temp_directory & = delete;
	auto operator=(temp_directory &&) -> temp_directory & = delete;
	~temp_directory()
	{
		std::error_code code;
		std::filesystem::remove_all(path, code);
	}
};
inline auto read(const std::filesystem::path &path) -> std::string
{
	std::ifstream file(path, std::ios::binary);
	require(static_cast<bool>(file));
	return {std::istreambuf_iterator<char>(file), {}};
}
inline void write(const std::filesystem::path &path, std::string_view text)
{
	std::ofstream file(path, std::ios::binary);
	file << text;
	file.close();
	require(static_cast<bool>(file));
}
} // namespace test
#endif
