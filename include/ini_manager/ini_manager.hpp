#include <algorithm>
#include <expected> // C++23
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ini
{

template <typename T>
concept StreamExtractable =
	requires(std::istream &istream, T &value) { istream >> value; };

constexpr auto trim(std::string_view str) noexcept -> std::string_view
{
	const size_t first = str.find_first_not_of(" \t\r\n");
	if (std::string_view::npos == first)
	{
		return {};
	}
	const size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, (last - first + 1));
}

struct section
{
	std::string_view value;
};

struct key
{
	std::string_view value;
};

class ini_manager
{
  public:
	ini_manager()
		: m_data(std::make_shared<
				 std::map<std::string, std::map<std::string, std::string>>>())
	{
	}

	static auto from_file(const std::string &file_path)
		-> std::expected<ini_manager, std::error_code>
	{
		ini_manager manager;
		auto result = manager.load(file_path);
		if (result.has_value())
		{
			manager.m_file_path = file_path;
			return manager;
		}
		return std::unexpected(result.error());
	}

	static auto from_stream(std::istream &istream)
		-> std::expected<ini_manager, std::error_code>
	{
		ini_manager manager;
		auto result = manager.parse(istream);
		if (result.has_value())
		{
			return manager;
		}
		return std::unexpected(result.error());
	}

	// Nested class for accessing keys within a section (non-const)
	class section_accessor
	{
	  public:
		explicit section_accessor(
			std::shared_ptr< // NOLINT(modernize-pass-by-value)
				std::map<std::string, std::map<std::string, std::string>>>
				data,
			std::string section_name)
			: m_data(data) // NOLINT(performance-unnecessary-value-param)
			  ,
			  m_section_name(std::move(section_name))
		{
		}

		auto operator[](std::string_view key) -> std::string &
		{
			return (*m_data)[m_section_name][std::string{key}];
		}

	  private:
		std::shared_ptr<std::map<std::string, std::map<std::string, std::string>>> m_data;
		std::string m_section_name;
	};

	// Nested class for accessing keys within a section (const)
	class const_section_accessor
	{
	  public:
		explicit const_section_accessor(
			std::shared_ptr< // NOLINT(modernize-pass-by-value)
				std::map<std::string, std::map<std::string, std::string>>>
				data,
			std::string section_name)
			: m_data(data) // NOLINT(performance-unnecessary-value-param)
			  ,
			  m_section_name(std::move(section_name))
		{
		}

		auto operator[](std::string_view key) const -> std::optional<std::string>
		{
			if (const auto it_section = (*m_data).find(m_section_name);
				it_section != (*m_data).cend())
			{
				if (const auto it_key = it_section->second.find(std::string{key});
					it_key != it_section->second.cend())
				{
					return it_key->second;
				}
			}
			return std::nullopt;
		}

	  private:
		std::shared_ptr<std::map<std::string, std::map<std::string, std::string>>> m_data;
		std::string m_section_name;
	};

	auto operator[](std::string_view section) -> section_accessor
	{
		return section_accessor{m_data, std::string{section}};
	}

	auto operator[](std::string_view section) const -> const_section_accessor
	{
		return const_section_accessor{m_data, std::string{section}};
	}

	auto get_value(section section, key key) const noexcept -> std::optional<std::string>
	{
		return (*this)[section.value][key.value];
	}

	template <typename T>
	auto get_value(section section, key key) const noexcept -> std::optional<T>
	{
		if (auto value_str = (*this)[section.value][key.value]; value_str.has_value())
		{
			if constexpr (std::is_same_v<T, std::string>)
			{
				return value_str;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				std::istringstream iss(*value_str);
				std::string lower_value;
				iss >> lower_value;
				std::ranges::transform(lower_value, lower_value.begin(), ::tolower);
				if (lower_value == "true" || lower_value == "1")
				{
					return true;
				}
				if (lower_value == "false" || lower_value == "0")
				{
					return false;
				}
				return std::nullopt;
			}
			else if constexpr (StreamExtractable<T>)
			{
				std::istringstream iss(*value_str);
				T value;
				if (iss >> value && iss.eof())
				{
					return value;
				}
			}
		}
		return std::nullopt;
	}

	auto get_value_or_default(section section, key key,
							  std::string default_value) const noexcept -> std::string
	{
		if (auto value = get_value(section, key))
		{
			return *value;
		}
		return default_value;
	}

	template <typename T>
	auto get_value_or_default(section section, key key, T default_value) const noexcept
		-> T
	{
		if (auto value = get_value<T>(section, key))
		{
			return *value;
		}
		return default_value;
	}

	template <typename T>
		requires std::formattable<T, char>
	void set_value(std::string_view section, std::string_view key, T value) noexcept
	{
		(*m_data)[std::string{section}][std::string{key}] = std::format("{}", value);
	}

	void set_section(const std::string &section) noexcept
	{
		if (m_data->find(section) == m_data->end())
		{
			// Create a new empty section only if it doesn't exist
			(*m_data)[section] = {};
		}
		// If the section already exists, do nothing.
	}

	auto remove_value(section section, key key) noexcept -> bool
	{
		const auto section_it = m_data->find(std::string{section.value});
		if (section_it != m_data->end())
		{
			return section_it->second.erase(std::string{key.value}) > 0;
		}
		return false;
	}

	auto remove_section(section section) noexcept -> bool
	{
		return m_data->erase(std::string{section.value}) > 0;
	}

	auto load_file(const std::string &file_path) -> std::expected<void, std::error_code>
	{
		m_data =
			std::make_shared<std::map<std::string, std::map<std::string, std::string>>>();
		m_file_path = file_path;
		return load(file_path);
	}

	auto load_stream(std::istream &istream) -> std::expected<void, std::error_code>
	{
		m_data =
			std::make_shared<std::map<std::string, std::map<std::string, std::string>>>();
		m_file_path.clear();
		return parse(istream);
	}

	auto add_from_stream(std::istream &istream) -> std::expected<void, std::error_code>
	{
		return parse(istream);
	}

	auto add_from_file(const std::string &file_path)
		-> std::expected<void, std::error_code>
	{
		return load(file_path);
	}

	auto write_file(const std::string &file_path) const
		-> std::expected<void, std::error_code>
	{
		std::ofstream file(file_path);
		if (!file.is_open())
		{
			return std::unexpected(std::error_code(errno, std::system_category()));
		}
		return write(file);
	}

	auto write_file() const -> std::expected<void, std::error_code>
	{
		if (m_file_path.empty())
		{
			return std::unexpected(std::error_code(
				static_cast<int>(std::errc::invalid_argument), std::generic_category()));
		}
		return write_file(m_file_path);
	}

	friend auto operator<<(std::ostream &ostream, const ini_manager &manager)
		-> std::ostream &
	{
		auto result = manager.write(ostream);
		if (!result.has_value())
		{
			ostream.setstate(std::ios::failbit);
		}
		return ostream;
	}

	friend auto operator>>(std::istream &istream, ini_manager &manager) -> std::istream &
	{
		auto result = manager.parse(istream);
		if (!result.has_value())
		{
			istream.setstate(std::ios::failbit);
		}
		return istream;
	}

  private:
	std::shared_ptr<std::map<std::string, std::map<std::string, std::string>>> m_data;
	std::string m_file_path;

	auto load(const std::string &file_path) -> std::expected<void, std::error_code>
	{
		std::ifstream file(file_path);
		if (!file.is_open())
		{
			return std::unexpected(std::error_code(errno, std::system_category()));
		}
		return parse(file);
	}

	auto parse(std::istream &istream) -> std::expected<void, std::error_code>
	{
		std::string line;
		std::optional<std::string> current_section;
		while (std::getline(istream, line))
		{
			const std::string_view line_view = trim(line);

			if (line_view.empty() || line_view.starts_with(';') ||
				line_view.starts_with('#'))
			{
				continue;
			}

			if (line_view.starts_with('[') && line_view.ends_with(']'))
			{
				current_section =
					std::string{trim(line_view.substr(1, line_view.length() - 2))};
				continue;
			}

			if (auto delimiter_pos = line_view.find('=');
				delimiter_pos != std::string_view::npos)
			{
				if (current_section.has_value())
				{
					auto key = trim(line_view.substr(0, delimiter_pos));
					auto value = trim(line_view.substr(delimiter_pos + 1));
					(*m_data)[*current_section][std::string{key}] = std::string{value};
				}
			}
			if (istream.fail() && !istream.eof())
			{
				return std::unexpected(std::error_code(errno, std::system_category()));
			}
		}
		return {};
	}

	auto write(std::ostream &ostream) const -> std::expected<void, std::error_code>
	{
		for (const auto &[section, entries] : (*m_data))
		{
			ostream << "[" << section << "]\n";
			for (const auto &[key, value] : entries)
			{
				ostream << key << " = " << value << "\n";
			}
			ostream << "\n";
		}
		if (ostream.fail())
		{
			return std::unexpected(std::error_code(errno, std::system_category()));
		}
		return {};
	}
};

} // namespace ini
