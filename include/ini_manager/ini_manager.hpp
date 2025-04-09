/**
 * @file ini_manager.hpp
 * @brief A header-only C++23 library for parsing, reading, and writing INI files.
 *
 * This library provides a simple and intuitive interface to manage
 * configuration settings in INI format within C++ applications.
 */

#ifndef INI_MANAGER_HPP
#define INI_MANAGER_HPP

#include <algorithm>
#include <expected>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ini
{

/**
 * @brief Concept that checks if a type `T` can be extracted from an input stream using
 * `operator>>`.
 * @tparam T The type to check.
 */
template <typename T>
concept StreamExtractable =
	requires(std::istream &istream, T &value) { istream >> value; };

/**
 * @brief Trims leading and trailing whitespace from a string view.
 * @param str The string view to trim.
 * @return A string view with leading and trailing whitespace removed.
 */
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

/**
 * @brief Represents a section name in an INI file.
 */
struct section
{
	/**
	 * @brief The string view representing the section name.
	 */
	std::string_view value;
};

/**
 * @brief Represents a key name in an INI file.
 */
struct key
{
	/**
	 * @brief The string view representing the key name.
	 */
	std::string_view value;
};

/**
 * @brief Manages INI file data, allowing reading, writing, and manipulation of
 * configuration settings.
 */
class ini_manager
{
	// Define the underlying data map type for brevity
	using data_map = std::map<std::string, std::map<std::string, std::string>>;

  public:
	/**
	 * @brief Default constructor for the ini_manager class.
	 *
	 * Initializes an empty INI configuration.
	 */
	ini_manager() : m_data(std::make_shared<data_map>())
	{
	}

	/**
	 * @brief Creates an ini_manager object by loading data from a file.
	 * @param file_path The path to the INI file.
	 * @return A `std::expected` containing the ini_manager object on success,
	 * or a `std::error_code` on failure.
	 */
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

	/**
	 * @brief Creates an ini_manager object by parsing data from an input stream.
	 * @param istream The input stream containing INI data.
	 * @return A `std::expected` containing the ini_manager object on success,
	 * or a `std::error_code` on failure.
	 */
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

	/**
	 * @brief Provides non-const access to keys within a specific section.
	 */
	class section_accessor
	{
	  public:
		/**
		 * @brief Constructs a section_accessor.
		 * @param data A shared pointer to the underlying data map.
		 * @param section_name The name of the section.
		 */
		explicit section_accessor(std::shared_ptr<data_map> data,
								  std::string section_name)
			: m_data(std::move(data)), m_section_name(std::move(section_name))
		{
		}

		/**
		 * @brief Accesses a key within the section for modification.
		 * If the key does not exist, it will be created.
		 * @param key The name of the key.
		 * @return A reference to the string value associated with the key.
		 */
		auto operator[](std::string_view key) -> std::string &
		{
			return (*m_data)[m_section_name][std::string{key}];
		}

	  private:
		std::shared_ptr<data_map> m_data;
		std::string m_section_name;
	};

	/**
	 * @brief Provides const access to keys within a specific section.
	 */
	class const_section_accessor
	{
	  public:
		/**
		 * @brief Constructs a const_section_accessor.
		 * @param data A shared pointer to the underlying data map (const).
		 * @param section_name The name of the section.
		 */
		explicit const_section_accessor(std::shared_ptr<data_map> data,
										std::string section_name)
			: m_data(std::move(data)), m_section_name(std::move(section_name))
		{
		}

		/**
		 * @brief Accesses a key within the section for reading.
		 * @param key The name of the key.
		 * @return A `std::optional` containing the string value associated with the key,
		 * or `std::nullopt` if the key or section does not exist.
		 */
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
		std::shared_ptr<data_map> m_data;
		std::string m_section_name;
	};

	/**
	 * @brief Provides non-const access to a specific section.
	 * @param section The name of the section.
	 * @return A `section_accessor` object for accessing keys within the section.
	 */
	auto operator[](std::string_view section) -> section_accessor
	{
		return section_accessor{m_data, std::string{section}};
	}

	/**
	 * @brief Provides const access to a specific section.
	 * @param section The name of the section.
	 * @return A `const_section_accessor` object for accessing keys within the section.
	 */
	auto operator[](std::string_view section) const -> const_section_accessor
	{
		return const_section_accessor{m_data, std::string{section}};
	}

	/**
	 * @brief Retrieves a string value for a given section and key.
	 * @param section The section containing the key.
	 * @param key The key whose value to retrieve.
	 * @return A `std::optional` containing the string value,
	 * or `std::nullopt` if the section or key does not exist.
	 */
	auto get_value(section section, key key) const noexcept -> std::optional<std::string>
	{
		return (*this)[section.value][key.value];
	}

	/**
	 * @brief Retrieves a value of a specific type for a given section and key.
	 * @tparam T The type of the value to retrieve. Must be `std::string`, `bool`,
	 * or satisfy the `StreamExtractable` concept.
	 * @param section The section containing the key.
	 * @param key The key whose value to retrieve.
	 * @return A `std::optional` containing the value of type `T`,
	 * or `std::nullopt` if the section or key does not exist, or if the
	 * value cannot be converted to the requested type.
	 * @concept StreamExtractable<T>
	 */
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
				// Use a local copy for modification
				std::string lower_value = *value_str;
				std::ranges::transform(lower_value, lower_value.begin(), ::tolower);
				// Trim potential whitespace around boolean value before comparison
				lower_value = std::string{trim(lower_value)};

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
				// Check for successful extraction AND that the entire string was consumed
				if ((iss >> value) && iss.eof())
				{
					return value;
				}
			}
		}
		return std::nullopt;
	}

	/**
	 * @brief Retrieves a string value for a given section and key, or a default value if
	 * not found.
	 * @param section The section containing the key.
	 * @param key The key whose value to retrieve.
	 * @param default_value The value to return if the section or key does not exist.
	 * @return The string value associated with the key, or the default value.
	 */
	auto get_value_or_default(section section, key key,
							  std::string default_value) const noexcept -> std::string
	{
		return get_value(section, key).value_or(std::move(default_value));
	}

	/**
	 * @brief Retrieves a value of a specific type for a given section and key,
	 * or a default value if not found.
	 * @tparam T The type of the value to retrieve. Must be `std::string`, `bool`,
	 * or satisfy the `StreamExtractable` concept.
	 * @param section The section containing the key.
	 * @param key The key whose value to retrieve.
	 * @param default_value The value to return if the section or key does not exist,
	 * or if the value cannot be converted to the requested type.
	 * @return The value of type `T` associated with the key, or the default value.
	 * @concept StreamExtractable<T>
	 */
	template <typename T>
	auto get_value_or_default(section section, key key, T default_value) const noexcept
		-> T
	{
		return get_value<T>(section, key).value_or(std::move(default_value));
	}

	/**
	 * @brief Sets a value for a given section and key.
	 * @tparam T The type of the value to set. Must be formattable using `std::format`.
	 * @param section The name of the section.
	 * @param key The name of the key.
	 * @param value The value to set.
	 * @requires std::formattable<T, char>
	 */
	template <typename T>
		requires std::formattable<T, char>
	void set_value(std::string_view section, std::string_view key, T value) noexcept
	{
		(*m_data)[std::string{section}][std::string{key}] = std::format("{}", value);
	}

	/**
	 * @brief Sets a section in the INI data. If the section does not exist, it is
	 * created.
	 * @param section The name of the section to set.
	 */
	void set_section(const std::string &section) noexcept
	{
		if (!m_data->contains(section))
		{
			// Create a new empty section only if it doesn't exist
			(*m_data)[section] = {};
		}
		// If the section already exists, do nothing.
	}

	/**
	 * @brief Removes a key-value pair from a section.
	 * @param section The section containing the key to remove.
	 * @param key The key to remove.
	 * @return `true` if the key was successfully removed, `false` otherwise.
	 */
	auto remove_value(section section, key key) noexcept -> bool
	{
		const auto section_it = m_data->find(std::string{section.value});
		if (section_it != m_data->end())
		{
			// erase returns the number of elements removed (0 or 1 for map)
			return section_it->second.erase(std::string{key.value}) > 0;
		}
		return false;
	}

	/**
	 * @brief Removes an entire section from the INI data.
	 * @param section The section to remove.
	 * @return `true` if the section was successfully removed, `false` otherwise.
	 */
	auto remove_section(section section) noexcept -> bool
	{
		return m_data->erase(std::string{section.value}) > 0;
	}

	/**
	 * @brief Gets a list of all section names in the INI data.
	 * @return A `std::vector` containing the names of all sections.
	 * The order corresponds to the map's internal ordering (typically
	 * alphabetical).
	 */
	auto get_sections() const -> std::vector<std::string>
	{
		auto key_view = std::views::keys(*m_data);
		return {key_view.begin(), key_view.end()};
	}

	/**
	 * @brief Gets a list of all key names within a specific section.
	 * @param section The section whose keys are to be retrieved.
	 * @return A `std::vector` containing the names of all keys in the specified section.
	 * Returns an empty vector if the section does not exist.
	 * The order corresponds to the map's internal ordering (typically
	 * alphabetical).
	 */
	auto get_keys(section section) const -> std::vector<std::string>
	{
		const auto section_it = m_data->find(std::string{section.value});
		if (section_it != m_data->end())
		{
			auto key_view = std::views::keys(section_it->second);
			return {key_view.begin(), key_view.end()};
		}
		// Section not found, return an empty vector
		return {};
	}

	/**
	 * @brief Loads INI data from a file, replacing any existing data.
	 * @param file_path The path to the INI file.
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 */
	auto load_file(const std::string &file_path) -> std::expected<void, std::error_code>
	{
		// Clear existing data and reset file path
		m_data = std::make_shared<data_map>();
		m_file_path = file_path;
		return load(file_path);
	}

	/**
	 * @brief Loads INI data from an input stream, replacing any existing data.
	 * @param istream The input stream containing INI data.
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 */
	auto load_stream(std::istream &istream) -> std::expected<void, std::error_code>
	{
		// Clear existing data and reset file path
		m_data = std::make_shared<data_map>();
		m_file_path.clear();
		return parse(istream);
	}

	/**
	 * @brief Adds INI data from an input stream to the existing data.
	 * Existing keys in existing sections will be overwritten. New sections/keys are
	 * added.
	 * @param istream The input stream containing INI data to add.
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 */
	auto add_from_stream(std::istream &istream) -> std::expected<void, std::error_code>
	{
		// Parse directly into the existing data
		return parse(istream);
	}

	/**
	 * @brief Adds INI data from a file to the existing data.
	 * Existing keys in existing sections will be overwritten. New sections/keys are
	 * added.
	 * @param file_path The path to the INI file to add.
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 */
	auto add_from_file(const std::string &file_path)
		-> std::expected<void, std::error_code>
	{
		// Load directly into the existing data
		return load(file_path);
	}

	/**
	 * @brief Writes the current INI data to a file.
	 * @param file_path The path to the file to write to.
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 */
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

	/**
	 * @brief Writes the current INI data to the file specified during loading (if any).
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 * Fails with `std::errc::operation_not_permitted` if no file path was previously
	 * loaded or set.
	 */
	auto write_file() const -> std::expected<void, std::error_code>
	{
		if (m_file_path.empty())
		{
			// Use a standard error code indicating invalid operation state
			return std::unexpected(
				std::error_code(static_cast<int>(std::errc::operation_not_permitted),
								std::generic_category()));
		}
		return write_file(m_file_path);
	}

	/**
	 * @brief Writes the INI data to an output stream.
	 * @param ostream The output stream to write to.
	 * @param manager The ini_manager object to write.
	 * @return A reference to the output stream. Sets failbit on error.
	 */
	friend auto operator<<(std::ostream &ostream, const ini_manager &manager)
		-> std::ostream &
	{
		auto result = manager.write(ostream);
		if (!result.has_value())
		{
			// Propagate failure to the stream state
			ostream.setstate(std::ios::failbit);
		}
		return ostream;
	}

	/**
	 * @brief Parses INI data from an input stream.
	 * @param istream The input stream to read from.
	 * @param manager The ini_manager object to populate.
	 * @return A reference to the input stream.
	 */
	friend auto operator>>(std::istream &istream, ini_manager &manager) -> std::istream &
	{
		auto result = manager.parse(istream);
		if (!result.has_value())
		{
			// Propagate failure to the stream state
			istream.setstate(std::ios::failbit);
		}
		return istream;
	}

  private:
	/**
	 * @brief The underlying data structure storing the INI configuration.
	 * The outer map represents sections, and the inner map represents key-value pairs
	 * within each section. Uses shared_ptr for potential copy efficiency if needed.
	 */
	std::shared_ptr<data_map> m_data;
	/**
	 * @brief The file path of the INI file, if loaded from or intended to be saved to a
	 * specific file.
	 */
	std::string m_file_path;

	/**
	 * @brief Loads INI data from a file, adding to or overwriting existing data.
	 * @param file_path The path to the INI file.
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 */
	auto load(const std::string &file_path) -> std::expected<void, std::error_code>
	{
		std::ifstream file(file_path);
		if (!file.is_open())
		{
			return std::unexpected(std::error_code(errno, std::system_category()));
		}
		return parse(file);
	}

	/**
	 * @brief Parses INI data from an input stream, adding to or overwriting existing
	 * data.
	 * @param istream The input stream to parse.
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 */
	auto parse(std::istream &istream) -> std::expected<void, std::error_code>
	{
		std::string line;
		std::optional<std::string> current_section;

		while (std::getline(istream, line))
		{
			const std::string_view line_view = trim(line);

			// Skip empty lines and comments
			if (line_view.empty() || line_view.starts_with(';') ||
				line_view.starts_with('#'))
			{
				continue;
			}

			// Section header: [SectionName]
			if (line_view.starts_with('[') && line_view.ends_with(']'))
			{
				if (line_view.length() < 3)
				{ // Handle empty section "[ ]" -> treat as unnamed or error?
				  // Current behavior: treats as empty string section name ""
					current_section = "";
				}
				else
				{
					current_section =
						std::string{trim(line_view.substr(1, line_view.length() - 2))};
				}
				// Ensure the section exists in the map (creates if new)
				set_section(*current_section);
				continue;
			}

			// Key-value pair: Key = Value
			if (auto delimiter_pos = line_view.find('=');
				delimiter_pos != std::string_view::npos)
			{
				if (current_section.has_value())
				{
					auto key = trim(line_view.substr(0, delimiter_pos));
					auto value = trim(line_view.substr(delimiter_pos + 1));
					// Check if key is empty
					if (!key.empty())
					{
						(*m_data)[*current_section][std::string{key}] =
							std::string{value};
					}
				}
			}

			// Check stream state *after* processing the line
			if (istream.fail() && !istream.eof())
			{
				return std::unexpected(std::error_code(EIO, std::system_category()));
			}
		}

		// Check final stream state after loop (e.g., if badbit is set without failbit)
		if (istream.bad())
		{
			return std::unexpected(std::error_code(EIO, std::system_category()));
		}

		return {};
	}

	/**
	 * @brief Writes the INI data to an output stream.
	 * @param ostream The output stream to write to.
	 * @return A `std::expected` indicating success or failure with an `std::error_code`.
	 */
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
		// Check stream state after writing all data
		if (ostream.fail())
		{
			return std::unexpected(std::error_code(errno, std::system_category()));
		}
		return {};
	}
};

} // namespace ini

#endif // INI_MANAGER_HPP
