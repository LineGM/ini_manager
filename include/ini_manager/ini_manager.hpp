/** @file ini_manager.hpp
 * Header-only INI storage with value semantics, strict parsing and checked I/O.
 * Requires C++26; see README.md for standard library feature requirements.
 */
#ifndef INI_MANAGER_HPP
#define INI_MANAGER_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <ios>
#include <istream>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

#if (defined(_MSVC_LANG) && _MSVC_LANG <= 202302L) ||                                    \
	(!defined(_MSVC_LANG) && __cplusplus <= 202302L)
#error "ini_manager requires C++26 mode (-std=c++26 or /std:c++latest)"
#endif
#if !defined(__cpp_lib_associative_heterogeneous_insertion) ||                           \
	__cpp_lib_associative_heterogeneous_insertion < 202306L
#error "ini_manager requires C++26 heterogeneous map insertion (P2363R5)"
#endif
#if !defined(__cpp_lib_to_chars) || __cpp_lib_to_chars < 202306L
#error "ini_manager requires C++26 charconv result testing (P2497R0)"
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define INI_MANAGER_UNDEF_NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef INI_MANAGER_UNDEF_NOMINMAX
#undef NOMINMAX
#undef INI_MANAGER_UNDEF_NOMINMAX
#endif
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ini
{

/// Borrowed call arguments: the pointed-to text must live through the call.
struct section
{
	std::string_view value;
};
struct key
{
	std::string_view value;
};

enum class duplicate_policy : std::uint8_t
{
	reject,
	last_wins,
};
struct parse_options
{
	bool allow_colon = false;
	bool inline_comments = false;
	duplicate_policy duplicates = duplicate_policy::reject;
	static constexpr std::size_t default_max_line_bytes = 64UZ * 1024;
	std::size_t max_line_bytes = default_max_line_bytes;
	static constexpr std::size_t default_max_input_bytes = 16UZ * 1024 * 1024;
	std::size_t max_input_bytes = default_max_input_bytes;
	static constexpr std::size_t default_max_sections = 4096;
	std::size_t max_sections = default_max_sections;
	static constexpr std::size_t default_max_keys = 100000;
	std::size_t max_keys = default_max_keys;
	auto operator==(const parse_options &) const -> bool = default;
};
enum class error_reason : std::uint8_t
{
	missing_section,
	missing_key,
	invalid_format,
	out_of_range,
	invalid_section,
	invalid_key,
	invalid_value,
	invalid_header,
	missing_separator,
	duplicate_key,
	invalid_control,
	resource_limit,
	io_error,
	no_file_path,
	invalid_target,
};
enum class operation : std::uint8_t
{
	parse,
	lookup,
	validate,
	open,
	read,
	write,
	flush,
	close,
	inspect,
	create_temp,
	permissions,
	replace,
	cleanup,
};
enum class replacement_state : std::uint8_t
{
	unchanged,
	replaced,
	unknown,
};

/// Owning diagnostic. line/column are one-based byte positions when available.
struct error
{
	error_reason reason = error_reason::io_error;
	operation op = operation::parse;
	std::optional<std::size_t> line;
	std::optional<std::size_t> column;
	std::string section_name;
	std::string key_name;
	std::optional<std::filesystem::path> path;
	std::error_code system_code;
	replacement_state replacement = replacement_state::unchanged;
	std::error_code cleanup_code;
	std::optional<std::filesystem::path> temporary_path;

	[[nodiscard]] auto message() const -> std::string
	{
		constexpr std::array reasons{
			"missing section",
			"missing key",
			"invalid value format",
			"value out of range",
			"unrepresentable section name",
			"unrepresentable key",
			"unrepresentable value",
			"invalid section header",
			"missing key/value separator",
			"duplicate key",
			"invalid control byte",
			"resource limit exceeded",
			"I/O failure",
			"no associated file path",
			"target is not a regular file",
		};
		constexpr std::array operations{
			"parse",		   "lookup",  "validate", "open",	 "read",
			"write",		   "flush",	  "close",	  "inspect", "create temporary file",
			"set permissions", "replace", "cleanup",
		};
		// error is a public aggregate: even an unrecognized enum value must be safe.
		const auto operation_index = std::to_underlying(op);
		const auto reason_index = std::to_underlying(reason);
		auto result = std::string(operation_index < operations.size()
									  ? operations.at(operation_index)
									  : "unknown operation");
		result += ": ";
		result +=
			reason_index < reasons.size() ? reasons.at(reason_index) : "unknown reason";
		if (line)
		{
			result += " at line " + std::to_string(*line);
		}
		if (column)
		{
			result += ", column " + std::to_string(*column);
		}
		if (path)
		{
			result += " (" + path->string() + ")";
		}
		if (!section_name.empty() || !key_name.empty())
		{
			result += " [" + section_name + "] " + key_name;
		}
		if (system_code)
		{
			result += ": " + system_code.message();
		}
		if (replacement == replacement_state::replaced)
		{
			result += "; target already replaced";
		}
		if (replacement == replacement_state::unknown)
		{
			result += "; replacement outcome unknown";
		}
		if (cleanup_code)
		{
			result += "; cleanup failed: " + cleanup_code.message();
		}
		return result;
	}
};

template <class T> using result = std::expected<T, error>;

[[nodiscard]] constexpr auto trim(std::string_view text) noexcept -> std::string_view
{
	const auto first = text.find_first_not_of(" \t");
	if (first == std::string_view::npos)
	{
		return {};
	}
	return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

namespace detail
{
using entries = std::map<std::string, std::string, std::less<>>;
using data_map = std::map<std::string, entries, std::less<>>;
inline auto make_error(error_reason reason, operation action) -> error
{
	error diagnostic;
	diagnostic.reason = reason;
	diagnostic.op = action;
	return diagnostic;
}
inline auto io_error(operation action,
					 std::error_code code = std::make_error_code(std::errc::io_error))
	-> error
{
	auto diagnostic = make_error(error_reason::io_error, action);
	diagnostic.system_code = code;
	return diagnostic;
}
constexpr auto control(char byte, bool allow_tab) noexcept -> bool
{
	const auto code = static_cast<unsigned char>(byte);
	constexpr unsigned char ascii_delete = 127;
	return code == ascii_delete || (code < ' ' && (!allow_tab || byte != '\t'));
}
constexpr auto comment(char byte) noexcept -> bool
{
	return byte == ';' || byte == '#';
}
constexpr auto inline_comment(std::string_view text) noexcept -> std::size_t
{
	for (std::size_t i = 0; i < text.size(); ++i)
	{
		if (comment(text.at(i)) &&
			(i == 0 || text.at(i - 1) == ' ' || text.at(i - 1) == '\t'))
		{
			return i;
		}
	}
	return std::string_view::npos;
}
constexpr auto valid_section(std::string_view text) noexcept -> bool
{
	return trim(text) == text && text.find_first_of("[]") == std::string_view::npos &&
		   std::ranges::none_of(text, [](char byte) -> bool {
			   return control(byte, /*allow_tab=*/false);
		   });
}
constexpr auto valid_key(std::string_view text, const parse_options &options) noexcept
	-> bool
{
	return !text.empty() && trim(text) == text && text.front() != '[' &&
		   !comment(text.front()) && !text.contains('=') &&
		   (!options.allow_colon || !text.contains(':')) &&
		   std::ranges::none_of(text, [](char byte) -> bool {
			   return control(byte, /*allow_tab=*/false);
		   });
}
constexpr auto valid_value(std::string_view text, const parse_options &options) noexcept
	-> bool
{
	return trim(text) == text &&
		   std::ranges::none_of(
			   text,
			   [](char byte) -> bool { return control(byte, /*allow_tab=*/true); }) &&
		   (!options.inline_comments || inline_comment(text) == std::string_view::npos);
}
inline auto validate_entry(section group, key entry_name, std::string_view value,
						   const parse_options &options) -> result<void>
{
	const auto text = group.value;
	const auto name = entry_name.value;
	error_reason reason{};
	if (!valid_section(text))
	{
		reason = error_reason::invalid_section;
	}
	else if (!valid_key(name, options))
	{
		reason = error_reason::invalid_key;
	}
	else if (!valid_value(value, options))
	{
		reason = error_reason::invalid_value;
	}
	else
	{
		return {};
	}
	auto diagnostic = make_error(reason, operation::validate);
	diagnostic.section_name = text;
	diagnostic.key_name = name;
	return std::unexpected(std::move(diagnostic));
}

// A document parser owns only the tentative document, never the manager's state.
class document_parser
{
	data_map m_data;
	std::string m_current;
	parse_options m_options;
	std::size_t m_keys = 0;
	std::size_t m_line_number = 1;

	auto header(std::string_view text, std::size_t column) -> result<void>
	{
		const auto end = text.find(']');
		if (end == std::string_view::npos)
		{
			return std::unexpected(diagnostic(error_reason::invalid_header, column));
		}
		auto tail = text.substr(end + 1);
		if (!tail.empty() && tail.front() != ' ' && tail.front() != '\t')
		{
			return std::unexpected(
				diagnostic(error_reason::invalid_header, column + end + 1));
		}
		tail = trim(tail);
		if (!tail.empty() && !comment(tail.front()))
		{
			return std::unexpected(
				diagnostic(error_reason::invalid_header, column + end + 1));
		}
		const auto name = trim(text.substr(1, end - 1));
		if (!valid_section(name))
		{
			return std::unexpected(diagnostic(error_reason::invalid_section, column + 1));
		}
		if (!m_data.contains(name) && m_data.size() >= m_options.max_sections)
		{
			return std::unexpected(diagnostic(error_reason::resource_limit, column));
		}
		m_current = name;
		m_data.try_emplace(m_current);
		return {};
	}

	auto store(std::string_view name, std::string_view value, std::size_t column)
		-> result<void>
	{
		auto group = m_data.find(m_current);
		if (group == m_data.end())
		{
			if (m_data.size() >= m_options.max_sections)
			{
				return std::unexpected(diagnostic(error_reason::resource_limit, column));
			}
			group = m_data.try_emplace(m_current).first;
		}
		const bool duplicate = group->second.contains(name);
		if (duplicate && m_options.duplicates == duplicate_policy::reject)
		{
			auto failure = diagnostic(error_reason::duplicate_key, column);
			failure.section_name = m_current;
			failure.key_name = name;
			return std::unexpected(std::move(failure));
		}
		if (!duplicate && m_keys >= m_options.max_keys)
		{
			return std::unexpected(diagnostic(error_reason::resource_limit, column));
		}
		group->second.insert_or_assign(name, std::string(value));
		if (!duplicate)
		{
			++m_keys;
		}
		return {};
	}

	auto entry(std::string_view text, std::size_t column) -> result<void>
	{
		const auto separator = text.find_first_of(m_options.allow_colon ? "=:" : "=");
		if (separator == std::string_view::npos)
		{
			return std::unexpected(diagnostic(error_reason::missing_separator, column));
		}
		const auto name = trim(text.substr(0, separator));
		auto value = text.substr(separator + 1);
		if (m_options.inline_comments)
		{
			value = value.substr(0, inline_comment(value));
		}
		value = trim(value);
		if (auto validated = validate_entry({m_current}, {name}, value, m_options);
			!validated)
		{
			auto failure = std::move(validated.error());
			failure.op = operation::parse;
			failure.line = m_line_number;
			failure.column = column;
			return std::unexpected(std::move(failure));
		}
		return store(name, value, column);
	}

  public:
	explicit document_parser(parse_options options) : m_options(options)
	{
	}
	auto diagnostic(error_reason reason, std::size_t column) const -> error
	{
		auto failure = make_error(reason, operation::parse);
		failure.line = m_line_number;
		failure.column = column;
		return failure;
	}
	auto process(std::string_view raw) -> result<void>
	{
		constexpr std::string_view bom = "\xEF\xBB\xBF";
		std::size_t offset = 0;
		if (m_line_number == 1 && raw.starts_with(bom))
		{
			raw.remove_prefix(bom.size());
			offset = bom.size();
		}
		for (std::size_t index = 0; index < raw.size(); ++index)
		{
			if (control(raw.at(index), /*allow_tab=*/true))
			{
				return std::unexpected(
					diagnostic(error_reason::invalid_control, index + offset + 1));
			}
		}
		const auto text = trim(raw);
		if (text.empty() || comment(text.front()))
		{
			return {};
		}
		const auto column =
			static_cast<std::size_t>(text.data() - raw.data()) + offset + 1;
		return text.front() == '[' ? header(text, column) : entry(text, column);
	}
	void next_line()
	{
		++m_line_number;
	}
	auto finish() && -> data_map
	{
		return std::move(m_data);
	}
	auto read_failure(error failure, std::size_t column) const -> error
	{
		failure.line = m_line_number;
		failure.column = column;
		return failure;
	}
};

// Bounded byte accumulation is separate from INI syntax and storage.
class document_reader
{
	document_parser m_parser;
	parse_options m_options;
	std::string m_line;
	std::size_t m_total = 0;
	bool m_carriage_return = false;

  public:
	explicit document_reader(parse_options options)
		: m_parser(options), m_options(options)
	{
	}
	auto consume(char byte) -> result<void>
	{
		if (m_total >= m_options.max_input_bytes)
		{
			return std::unexpected(
				m_parser.diagnostic(error_reason::resource_limit, m_line.size() + 1));
		}
		++m_total;
		if (m_carriage_return && byte != '\n')
		{
			return std::unexpected(
				m_parser.diagnostic(error_reason::invalid_control, m_line.size() + 1));
		}
		if (byte == '\n')
		{
			if (auto processed = m_parser.process(m_line); !processed)
			{
				return processed;
			}
			m_line.clear();
			m_carriage_return = false;
			m_parser.next_line();
		}
		else if (byte == '\r')
		{
			m_carriage_return = true;
		}
		else
		{
			if (m_line.size() >= m_options.max_line_bytes)
			{
				return std::unexpected(
					m_parser.diagnostic(error_reason::resource_limit, m_line.size() + 1));
			}
			m_line.push_back(byte);
		}
		return {};
	}
	auto finish() && -> result<data_map>
	{
		if (m_carriage_return)
		{
			return std::unexpected(
				m_parser.diagnostic(error_reason::invalid_control, m_line.size() + 1));
		}
		if (!m_line.empty())
		{
			if (auto processed = m_parser.process(m_line); !processed)
			{
				return std::unexpected(std::move(processed.error()));
			}
		}
		return std::move(m_parser).finish();
	}
	auto read_failure(error failure) const -> error
	{
		return m_parser.read_failure(std::move(failure), m_line.size() + 1);
	}
};

// Copy the small source callable; it is invoked repeatedly and never consumed.
template <class Next>
auto parse(Next next, const parse_options &options) -> result<data_map>
{
	document_reader reader(options);
	for (;;)
	{
		auto byte = next();
		if (!byte)
		{
			return std::unexpected(reader.read_failure(std::move(byte.error())));
		}
		if (!*byte)
		{
			return std::move(reader).finish();
		}
		if (auto consumed = reader.consume(**byte); !consumed)
		{
			return std::unexpected(std::move(consumed.error()));
		}
	}
}

inline void set_state(std::ios &stream, std::ios::iostate state)
{
	try
	{
		stream.setstate(state);
	}
	catch (const std::ios_base::failure &)
	{
		// The flags are already set. Expected-based callers report the I/O error;
		// stream operators subsequently honor the original exception mask.
		return;
	}
}
inline auto parse_stream(std::istream &stream, const parse_options &options)
	-> result<data_map>
{
	if (stream.fail() || (stream.rdbuf() == nullptr))
	{
		return std::unexpected(io_error(operation::read));
	}
	return parse(
		[&] -> result<std::optional<char>> {
			if (stream.eof())
			{
				return std::optional<char>{};
			}
			using traits = std::char_traits<char>;
			traits::int_type byte = 0;
			try
			{
				byte = stream.rdbuf()->sbumpc();
			}
			catch (const std::bad_alloc &)
			{
				throw;
			}
			catch (const std::system_error &diagnostic)
			{
				set_state(stream, std::ios::badbit);
				return std::unexpected(io_error(
					operation::read, diagnostic.code()
										 ? diagnostic.code()
										 : std::make_error_code(std::errc::io_error)));
			}
			catch (...)
			{
				set_state(stream, std::ios::badbit);
				return std::unexpected(io_error(operation::read));
			}
			if (traits::eq_int_type(byte, traits::eof()))
			{
				set_state(stream, std::ios::eofbit);
				return std::optional<char>{};
			}
			return std::optional<char>{traits::to_char_type(byte)};
		},
		options);
}

template <class Append>
auto serialize_entries(std::string_view section_name, const entries &values,
					   const parse_options &options, Append &append) -> result<void>
{
	const auto limit_error = [] -> std::unexpected<error> {
		return std::unexpected(
			make_error(error_reason::resource_limit, operation::validate));
	};
	for (const auto &[key_name, value] : values)
	{

		if (auto outcome = validate_entry({section_name}, {key_name}, value, options);
			!outcome)
		{
			return std::unexpected(std::move(outcome.error()));
		}
		if (options.max_line_bytes < 3 || key_name.size() > options.max_line_bytes - 3 ||
			value.size() > options.max_line_bytes - 3 - key_name.size())
		{
			return limit_error();
		}
		if (!append(key_name) || !append(" = ") || !append(value) || !append("\n"))
		{
			return limit_error();
		}
	}
	return {};
}

inline auto serialize(const data_map &data, const parse_options &options)
	-> result<std::string>
{
	std::string output;
	const auto limit_error = [] -> std::unexpected<ini::error> {
		return std::unexpected(
			make_error(error_reason::resource_limit, operation::validate));
	};
	if (data.size() > options.max_sections)
	{
		return limit_error();
	}
	std::size_t keys = 0;
	auto append = [&](std::string_view section_name) -> bool {
		if (section_name.size() > options.max_input_bytes - output.size())
		{
			return false;
		}
		output.append(section_name);
		return true;
	};
	for (const auto &[section_name, entries] : data)
	{
		if (!valid_section(section_name))
		{
			return std::unexpected(
				make_error(error_reason::invalid_section, operation::validate));
		}
		if (options.max_line_bytes < 2 ||
			section_name.size() > options.max_line_bytes - 2)
		{
			return limit_error();
		}
		if (!append("[") || !append(section_name) || !append("]\n"))
		{
			return limit_error();
		}
		if (entries.size() > options.max_keys - keys)
		{
			return limit_error();
		}
		keys += entries.size();
		if (auto written = serialize_entries(section_name, entries, options, append);
			!written)
		{
			return std::unexpected(std::move(written.error()));
		}
		if (!append("\n"))
		{
			return limit_error();
		}
	}
	return output;
}

inline auto native_error() noexcept -> std::error_code
{
#ifdef _WIN32
	return {static_cast<int>(GetLastError()), std::system_category()};
#else
	return {errno, std::generic_category()};
#endif
}

// Small native adapter: exclusive creation, real error codes, explicit close.
class native_file
{
#ifdef _WIN32
	HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
	int m_handle = -1;
#endif
  public:
	native_file() = default;
	native_file(const native_file &) = delete;
	native_file(native_file &&) = delete;
	auto operator=(native_file &&) -> native_file & = delete;
	auto operator=(const native_file &) -> native_file & = delete;
	~native_file()
	{
		// Destructors cannot report a close error; explicit close is checked by callers.
		(void)close(); // NOLINT(bugprone-unused-return-value)
	}
	auto open(const std::filesystem::path &path, bool writing) -> result<void>
	{
#ifdef _WIN32
		m_handle = CreateFileW(
			path.c_str(), writing ? GENERIC_WRITE : GENERIC_READ,
			writing ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			writing ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		const bool failed = m_handle == INVALID_HANDLE_VALUE;
#else
		// POSIX open requires its variadic mode argument when O_CREAT is set.
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
		m_handle = ::open(path.c_str(),
						  writing ? O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW
								  : O_RDONLY | O_CLOEXEC,
						  S_IRUSR | S_IWUSR);
		const bool failed = m_handle == -1;
#endif
		if (failed)
		{
			return std::unexpected(io_error(
				writing ? operation::create_temp : operation::open, native_error()));
		}
		return {};
	}
	auto read(char *buffer, std::size_t size) const -> result<std::size_t>
	{
#ifdef _WIN32
		DWORD count = 0;
		if (!ReadFile(m_handle, buffer, static_cast<DWORD>(size), &count, nullptr))
			return std::unexpected(io_error(operation::read, native_error()));
#else
		auto count = ::read(m_handle, buffer, size);
		while (count < 0 && errno == EINTR)
		{
			count = ::read(m_handle, buffer, size);
		}
		if (count < 0)
		{
			return std::unexpected(io_error(operation::read, native_error()));
		}
#endif
		return static_cast<std::size_t>(count);
	}
	auto write(std::string_view bytes) const -> result<void>
	{
		while (!bytes.empty())
		{
			const auto amount = (std::min)(bytes.size(), 1024UZ * 1024);
#ifdef _WIN32
			DWORD count = 0;
			if (!WriteFile(m_handle, bytes.data(), static_cast<DWORD>(amount), &count,
						   nullptr))
				return std::unexpected(io_error(operation::write, native_error()));
#else
			auto count = ::write(m_handle, bytes.data(), amount);
			while (count < 0 && errno == EINTR)
			{
				count = ::write(m_handle, bytes.data(), amount);
			}
			if (count < 0)
			{
				return std::unexpected(io_error(operation::write, native_error()));
			}
#endif
			if (count == 0)
			{
				return std::unexpected(io_error(operation::write));
			}
			bytes.remove_prefix(static_cast<std::size_t>(count));
		}
		return {};
	}
	auto permissions(std::filesystem::perms mode) const -> result<void>
	{
#ifndef _WIN32
		if (::fchmod(m_handle, static_cast<mode_t>(mode & std::filesystem::perms::all)) !=
			0)
		{
			return std::unexpected(io_error(operation::permissions, native_error()));
		}
#else
		(void)mode;
#endif
		return {};
	}
	auto close() noexcept -> result<void>
	{
#ifdef _WIN32
		if (m_handle == INVALID_HANDLE_VALUE)
			return {};
		const auto handle = std::exchange(m_handle, INVALID_HANDLE_VALUE);
		const bool failed = !CloseHandle(handle);
#else
		if (m_handle == -1)
		{
			return {};
		}
		const auto handle = std::exchange(m_handle, -1);
		// Never retry close(EINTR): the descriptor may already have been released.
		const bool failed = ::close(handle) != 0;
#endif
		if (failed)
		{
			return std::unexpected(io_error(operation::close, native_error()));
		}
		return {};
	}
};

inline auto valid_path(const std::filesystem::path &path) noexcept -> bool
{
	return !path.empty() && !path.native().contains(std::filesystem::path::value_type{});
}
inline auto absolute_path(const std::filesystem::path &path, operation action)
	-> result<std::filesystem::path>
{
	if (!valid_path(path))
	{
		auto diagnostic = make_error(error_reason::invalid_target, action);
		diagnostic.path = path;
		return std::unexpected(std::move(diagnostic));
	}
	std::error_code code;
	auto resolved = std::filesystem::absolute(path, code);
	if (code)
	{
		auto diagnostic = io_error(action, code);
		diagnostic.path = path;
		return std::unexpected(std::move(diagnostic));
	}
	return resolved;
}
inline auto parse_file(const std::filesystem::path &path, const parse_options &options)
	-> result<data_map>
{
	if (!valid_path(path))
	{
		return std::unexpected(make_error(error_reason::invalid_target, operation::open));
	}
	native_file file;
	if (auto outcome = file.open(path, /*writing=*/false); !outcome)
	{
		return std::unexpected(std::move(outcome.error()));
	}
	std::array<char, 4096> buffer{};
	std::size_t position = 0;
	std::size_t available = 0;
	auto parsed = parse(
		[&] -> result<std::optional<char>> {
			if (position == available)
			{
				auto outcome = file.read(buffer.data(), buffer.size());
				if (!outcome)
				{
					return std::unexpected(std::move(outcome.error()));
				}
				position = 0;
				available = *outcome;
				if (!available)
				{
					return std::optional<char>{};
				}
			}
			return std::optional<char>{buffer.at(position++)};
		},
		options);
	auto closed = file.close();
	if (!parsed)
	{
		return parsed;
	}
	if (!closed)
	{
		return std::unexpected(std::move(closed.error()));
	}
	return parsed;
}

struct file_ops
{
	native_file file;
	std::filesystem::path target, temporary;
	std::optional<std::filesystem::perms> mode;
	explicit file_ops(std::filesystem::path path) : target(std::move(path))
	{
	}
	auto inspect() -> result<void>
	{
		if (!valid_path(target))
		{
			return std::unexpected(
				make_error(error_reason::invalid_target, operation::inspect));
		}
		std::error_code code;
		const auto status = std::filesystem::symlink_status(target, code);
		if (code && code != std::errc::no_such_file_or_directory)
		{
			return std::unexpected(io_error(operation::inspect, code));
		}
		if (status.type() == std::filesystem::file_type::not_found)
		{
			return {};
		}
		if (!std::filesystem::is_regular_file(status))
		{
			return std::unexpected(
				make_error(error_reason::invalid_target, operation::inspect));
		}
		mode = status.permissions();
		return {};
	}
	auto create() -> result<bool>
	{
		static std::atomic<std::uint64_t> sequence{0};
#ifdef _WIN32
		const auto process = GetCurrentProcessId();
#else
		const auto process = ::getpid();
#endif
		temporary =
			target.parent_path() /
			(".ini-manager-" + std::to_string(process) + "-" +
			 std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)) + ".tmp");
		auto outcome = file.open(temporary, /*writing=*/true);
		if (outcome)
		{
			return true;
		}
#ifdef _WIN32
		if (outcome.error().system_code.value() == ERROR_FILE_EXISTS ||
			outcome.error().system_code.value() == ERROR_ALREADY_EXISTS)
			return false;
#else
		if (outcome.error().system_code == std::errc::file_exists)
		{
			return false;
		}
#endif
		return std::unexpected(std::move(outcome.error()));
	}
	auto write(std::string_view bytes) const -> result<void>
	{
		return file.write(bytes);
	}
	auto permissions() const -> result<void>
	{
		return mode ? file.permissions(*mode) : result<void>{};
	}
	auto close() -> result<void>
	{
		return file.close();
	}
	auto replace() const -> result<void>
	{
#ifdef _WIN32
		if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING))
		{
			auto diagnostic = io_error(operation::replace, native_error());
			diagnostic.replacement = replacement_state::unknown;
			return std::unexpected(std::move(diagnostic));
		}
#else
		if (::rename(temporary.c_str(), target.c_str()) != 0)
		{
			auto diagnostic = io_error(operation::replace, native_error());
			if (diagnostic.system_code == std::errc::io_error)
			{
				diagnostic.replacement = replacement_state::unknown;
			}
			return std::unexpected(std::move(diagnostic));
		}
#endif
		return {};
	}
	auto cleanup() noexcept -> std::error_code
	{
		// Preserve the original failure; this is best-effort cleanup.
		(void)file.close(); // NOLINT(bugprone-unused-return-value)
#ifdef _WIN32
		if (!DeleteFileW(temporary.c_str()))
			return native_error();
#else
		if (::unlink(temporary.c_str()) != 0)
		{
			return native_error();
		}
#endif
		return {};
	}
};

// Internal injectable operation sequence, also used by deterministic failure tests.
template <class Ops> auto atomic_write(Ops &ops, std::string_view bytes) -> result<void>
{
	if (auto outcome = ops.inspect(); !outcome)
	{
		return outcome;
	}
	bool owned = false;
	struct cleanup_guard
	{
		Ops *ops;
		bool *owned;
		cleanup_guard(Ops &operations, bool &active) : ops(&operations), owned(&active)
		{
		}
		cleanup_guard(const cleanup_guard &) = delete;
		cleanup_guard(cleanup_guard &&) = delete;
		auto operator=(const cleanup_guard &) -> cleanup_guard & = delete;
		auto operator=(cleanup_guard &&) -> cleanup_guard & = delete;
		~cleanup_guard()
		{
			if (*owned)
			{
				(void)ops->cleanup();
			}
		}
	};
	const cleanup_guard guard{ops, owned};
	constexpr unsigned max_attempts = 128;
	for (unsigned attempt = 0; attempt < max_attempts; ++attempt)
	{
		auto outcome = ops.create();
		if (!outcome)
		{
			return std::unexpected(std::move(outcome.error()));
		}
		if (*outcome)
		{
			owned = true;
			break;
		}
	}
	if (!owned)
	{
		return std::unexpected(io_error(operation::create_temp,
										std::make_error_code(std::errc::file_exists)));
	}
	const auto fail = [&](error diagnostic) -> result<void> {
		diagnostic.cleanup_code = ops.cleanup();
		owned = false;
		if (diagnostic.cleanup_code)
		{
			diagnostic.temporary_path = ops.temporary;
		}
		return std::unexpected(std::move(diagnostic));
	};
	if (auto outcome = ops.write(bytes); !outcome)
	{
		return fail(std::move(outcome.error()));
	}
	if (auto outcome = ops.permissions(); !outcome)
	{
		return fail(std::move(outcome.error()));
	}
	if (auto outcome = ops.close(); !outcome)
	{
		return fail(std::move(outcome.error()));
	}
	if (auto outcome = ops.replace(); !outcome)
	{
		return fail(std::move(outcome.error()));
	}
	owned = false;
	return {};
}

template <class T>
inline constexpr bool integer =
	std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> &&
	!std::same_as<T, wchar_t> && !std::same_as<T, char8_t> &&
	!std::same_as<T, char16_t> && !std::same_as<T, char32_t>;
template <class T>
concept custom_readable =
	!std::is_arithmetic_v<T> && !std::is_pointer_v<T> &&
	!std::same_as<T, std::string_view> && std::default_initializable<T> &&
	std::move_constructible<T> && requires(std::istream &input, T &value) {
		{ input >> value } -> std::same_as<std::istream &>;
	};
} // namespace detail

template <class T>
concept readable_value =
	std::same_as<T, std::remove_cvref_t<T>> &&
	(std::same_as<T, std::string> || std::same_as<T, bool> || std::same_as<T, char> ||
	 detail::integer<T> || std::floating_point<T> || detail::custom_readable<T>);
template <class T>
concept writable_value =
	std::convertible_to<T, std::string_view> || detail::integer<std::remove_cvref_t<T>> ||
	std::floating_point<std::remove_cvref_t<T>> ||
	std::same_as<std::remove_cvref_t<T>, bool> ||
	std::same_as<std::remove_cvref_t<T>, char> ||
	(!std::is_arithmetic_v<std::remove_cvref_t<T>> &&
	 !std::is_pointer_v<std::remove_cvref_t<T>> && std::formattable<T, char>);

namespace detail
{
constexpr auto ascii_lower(char byte) noexcept -> char
{
	return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte + ('a' - 'A')) : byte;
}
constexpr auto ascii_equal(std::string_view left, std::string_view right) noexcept -> bool
{
	return std::ranges::equal(left, right, [](char first, char second) -> bool {
		return ascii_lower(first) == second;
	});
}
inline auto read_bool(std::string_view text) -> result<bool>
{
	const auto value = trim(text);
	if (ascii_equal(value, "true") || value == "1")
	{
		return true;
	}
	if (ascii_equal(value, "false") || value == "0")
	{
		return false;
	}
	return std::unexpected(make_error(error_reason::invalid_format, operation::lookup));
}
template <class T> auto read_number(std::string_view text) -> result<T>
{
	const auto value = trim(text);
	if (value.empty())
	{
		return std::unexpected(
			make_error(error_reason::invalid_format, operation::lookup));
	}
	if constexpr (std::is_unsigned_v<T>)
	{
		if (value.front() == '-')
		{
			return std::unexpected(
				make_error(error_reason::invalid_format, operation::lookup));
		}
	}
	T converted{};
	const auto *end = std::to_address(value.end());
	const auto conversion =
		std::from_chars(std::to_address(value.begin()), end, converted);
	if (conversion.ptr != end)
	{
		return std::unexpected(
			make_error(error_reason::invalid_format, operation::lookup));
	}
	if (conversion.ec == std::errc::result_out_of_range)
	{
		return std::unexpected(make_error(error_reason::out_of_range, operation::lookup));
	}
	if (!conversion)
	{
		return std::unexpected(
			make_error(error_reason::invalid_format, operation::lookup));
	}
	if constexpr (std::floating_point<T>)
	{
		if (!std::isfinite(converted))
		{
			return std::unexpected(
				make_error(error_reason::invalid_format, operation::lookup));
		}
	}
	return converted;
}
template <custom_readable T> auto read_custom(const std::string &text) -> result<T>
{
	std::istringstream input(text);
	input.imbue(std::locale::classic());
	T converted{};
	if (input >> converted)
	{
		auto *rest = input.rdbuf();
		using traits = std::char_traits<char>;
		for (auto byte = rest->sbumpc(); !traits::eq_int_type(byte, traits::eof());
			 byte = rest->sbumpc())
		{
			if (traits::to_char_type(byte) != ' ' && traits::to_char_type(byte) != '\t')
			{
				return std::unexpected(
					make_error(error_reason::invalid_format, operation::lookup));
			}
		}
		return converted;
	}
	return std::unexpected(make_error(error_reason::invalid_format, operation::lookup));
}
template <readable_value T> auto read_value(const std::string &text) -> result<T>
{
	if constexpr (std::same_as<T, std::string>)
	{
		return text;
	}
	else if constexpr (std::same_as<T, char>)
	{
		if (text.size() == 1)
		{
			return text.front();
		}
		return std::unexpected(
			make_error(error_reason::invalid_format, operation::lookup));
	}
	else if constexpr (std::same_as<T, bool>)
	{
		return read_bool(text);
	}
	else if constexpr (integer<T> || std::floating_point<T>)
	{
		return read_number<T>(text);
	}
	else
	{
		return read_custom<T>(text);
	}
}
template <class S> auto string_text(S &&source, operation action) -> result<std::string>
{
	using value_type = std::remove_cvref_t<S>;
	if constexpr (std::is_pointer_v<value_type>)
	{
		if (source == nullptr)
		{
			return std::unexpected(make_error(error_reason::invalid_value, action));
		}
	}
	if constexpr (std::is_array_v<value_type>)
	{
		const auto bytes = std::span(source);
		const auto length =
			!bytes.empty() && bytes.back() == '\0' ? bytes.size() - 1 : bytes.size();
		const auto content = bytes.first(length);
		return std::string(content.begin(), content.end());
	}
	else
	{
		return std::string(std::string_view(std::forward<S>(source)));
	}
}
template <class T> auto number_text(T value) -> result<std::string>
{
	if constexpr (std::floating_point<T>)
	{
		if (!std::isfinite(value))
		{
			return std::unexpected(
				make_error(error_reason::invalid_value, operation::validate));
		}
	}
	constexpr std::size_t buffer_size = 128;
	std::array<char, buffer_size> buffer{};
	const auto converted =
		std::to_chars(buffer.data(), std::to_address(buffer.end()), value);
	if (!converted)
	{
		return std::unexpected(
			make_error(error_reason::out_of_range, operation::validate));
	}
	return std::string(buffer.data(), converted.ptr);
}
template <writable_value T> auto value_text(T &&value) -> result<std::string>
{
	using value_type = std::remove_cvref_t<T>;
	if constexpr (std::convertible_to<T, std::string_view>)
	{
		return string_text(std::forward<T>(value), operation::validate);
	}
	else if constexpr (std::same_as<value_type, bool>)
	{
		return value ? "true" : "false";
	}
	else if constexpr (std::same_as<value_type, char>)
	{
		return std::string(1, value);
	}
	else if constexpr (integer<value_type> || std::floating_point<value_type>)
	{
		return number_text(value);
	}
	else
	{
		return std::format("{}", std::forward<T>(value));
	}
}
} // namespace detail

/// Value object. No accessor, iterator, or reference into its storage is exposed.
class ini_manager
{
	detail::data_map m_data;
	std::optional<std::filesystem::path> m_path;
	parse_options m_options;

	auto lookup(section group, key name) const -> result<const std::string *>
	{
		const auto sec = m_data.find(group.value);
		if (sec != m_data.end())
		{
			const auto entry = sec->second.find(name.value);
			if (entry != sec->second.end())
			{
				return &entry->second;
			}
		}
		auto diagnostic =
			detail::make_error(sec == m_data.end() ? error_reason::missing_section
												   : error_reason::missing_key,
							   operation::lookup);
		diagnostic.section_name = group.value;
		diagnostic.key_name = name.value;
		return std::unexpected(std::move(diagnostic));
	}
	auto merge(detail::data_map incoming) -> result<void>
	{
		auto candidate = m_data;
		for (auto &[group, entries] : incoming)
		{
			auto &destination = candidate[group];
			for (auto &[name, value] : entries)
			{
				destination.insert_or_assign(name, std::move(value));
			}
		}
		// Check resulting counts and canonical resource limits before committing.
		if (auto valid = detail::serialize(candidate, m_options); !valid)
		{
			return std::unexpected(std::move(valid.error()));
		}
		m_data.swap(candidate);
		return {};
	}

  public:
	explicit ini_manager(parse_options options = {}) : m_options(options)
	{
	}
	~ini_manager() = default;
	ini_manager(const ini_manager &) = default;
	// Some standard libraries allocate a map sentinel for the empty source.
	ini_manager(ini_manager &&other) noexcept(
		std::is_nothrow_default_constructible_v<detail::data_map>)
		: ini_manager()
	{
		swap(other);
	}
	auto operator=(const ini_manager &other) -> ini_manager &
	{
		if (this != &other)
		{
			ini_manager copy(other);
			swap(copy);
		}
		return *this;
	}
	auto operator=(ini_manager &&other) noexcept(
		std::is_nothrow_default_constructible_v<detail::data_map>) -> ini_manager &
	{
		if (this != &other)
		{
			ini_manager moved(std::move(other));
			swap(moved);
		}
		return *this;
	}
	void swap(ini_manager &other) noexcept
	{
		m_data.swap(other.m_data);
		m_path.swap(other.m_path);
		std::swap(m_options, other.m_options);
	}
	friend void swap(ini_manager &left, ini_manager &right) noexcept
	{
		left.swap(right);
	}
	[[nodiscard]] auto options() const noexcept -> parse_options
	{
		return m_options;
	}
	[[nodiscard]] auto file_path() const -> std::optional<std::filesystem::path>
	{
		return m_path;
	}

	[[nodiscard]] static auto from_file(const std::filesystem::path &path,
										parse_options options = {}) -> result<ini_manager>
	{
		ini_manager manager;
		if (auto outcome = manager.load_file(path, options); !outcome)
		{
			return std::unexpected(std::move(outcome.error()));
		}
		return manager;
	}
	[[nodiscard]] static auto from_stream(std::istream &stream,
										  parse_options options = {})
		-> result<ini_manager>
	{
		ini_manager manager;
		if (auto outcome = manager.load_stream(stream, options); !outcome)
		{
			return std::unexpected(std::move(outcome.error()));
		}
		return manager;
	}
	[[nodiscard]] auto load_file(const std::filesystem::path &path,
								 parse_options options = {}) -> result<void>
	{
		auto resolved = detail::absolute_path(path, operation::open);
		if (!resolved)
		{
			return std::unexpected(std::move(resolved.error()));
		}
		auto parsed = detail::parse_file(*resolved, options);
		if (!parsed)
		{
			parsed.error().path = *resolved;
			return std::unexpected(std::move(parsed.error()));
		}
		ini_manager candidate(options);
		candidate.m_data = std::move(*parsed);
		candidate.m_path = std::move(*resolved);
		swap(candidate);
		return {};
	}
	[[nodiscard]] auto load_stream(std::istream &stream, parse_options options = {})
		-> result<void>
	{
		auto parsed = detail::parse_stream(stream, options);
		if (!parsed)
		{
			return std::unexpected(std::move(parsed.error()));
		}
		ini_manager candidate(options);
		candidate.m_data = std::move(*parsed);
		swap(candidate);
		return {};
	}
	[[nodiscard]] auto add_from_file(const std::filesystem::path &path) -> result<void>
	{
		auto resolved = detail::absolute_path(path, operation::open);
		if (!resolved)
		{
			return std::unexpected(std::move(resolved.error()));
		}
		auto parsed = detail::parse_file(*resolved, m_options);
		if (!parsed)
		{
			parsed.error().path = *resolved;
			return std::unexpected(std::move(parsed.error()));
		}
		auto outcome = merge(std::move(*parsed));
		if (!outcome)
		{
			outcome.error().path = *resolved;
		}
		return outcome;
	}
	[[nodiscard]] auto add_from_stream(std::istream &stream) -> result<void>
	{
		auto parsed = detail::parse_stream(stream, m_options);
		if (!parsed)
		{
			return std::unexpected(std::move(parsed.error()));
		}
		return merge(std::move(*parsed));
	}

	[[nodiscard]] auto get_value(section group, key name) const -> result<std::string>
	{
		return get_value<std::string>(group, name);
	}
	template <readable_value T>
	[[nodiscard]] auto get_value(section group, key name) const -> result<T>
	{
		auto found = lookup(group, name);
		if (!found)
		{
			return std::unexpected(std::move(found.error()));
		}
		auto converted = detail::read_value<T>(**found);
		if (!converted)
		{
			converted.error().section_name = group.value;
			converted.error().key_name = name.value;
		}
		return converted;
	}

	template <class S>
		requires std::convertible_to<S, std::string_view>
	[[nodiscard]] auto get_value_or_default(section group, key name, S &&fallback) const
		-> result<std::string>
	{
		auto outcome = get_value(group, name);
		if (!outcome && (outcome.error().reason == error_reason::missing_section ||
						 outcome.error().reason == error_reason::missing_key))
		{
			return detail::string_text(std::forward<S>(fallback), operation::lookup);
		}
		return outcome;
	}

	template <readable_value T>
		requires(!std::convertible_to<T, std::string_view>)
	[[nodiscard]] auto get_value_or_default(section group, key name, T fallback) const
		-> result<T>
	{
		auto outcome = get_value<T>(group, name);
		if (!outcome && (outcome.error().reason == error_reason::missing_section ||
						 outcome.error().reason == error_reason::missing_key))
		{
			return fallback;
		}
		return outcome;
	}

	template <writable_value T>
	[[nodiscard]] auto set_value(section group, key name, T &&value) -> result<void>
	{
		auto formatted = detail::value_text(std::forward<T>(value));
		if (!formatted)
		{
			return std::unexpected(std::move(formatted.error()));
		}
		auto text = std::move(*formatted);
		if (auto valid = detail::validate_entry(group, name, text, m_options); !valid)
		{
			return valid;
		}
		// Prepare all potentially throwing work before touching the stored map.
		const auto sec = m_data.find(group.value);
		if (sec == m_data.end() && m_data.size() >= m_options.max_sections)
		{
			return std::unexpected(
				detail::make_error(error_reason::resource_limit, operation::validate));
		}
		const bool new_key = sec == m_data.end() || !sec->second.contains(name.value);
		if (new_key)
		{
			std::size_t count = 0;
			for (const auto &entry : m_data)
			{
				count += entry.second.size();
			}
			if (count >= m_options.max_keys)
			{
				return std::unexpected(detail::make_error(error_reason::resource_limit,
														  operation::validate));
			}
		}
		if (sec == m_data.end())
		{
			detail::entries values;
			values.try_emplace(name.value, std::move(text));
			m_data.try_emplace(group.value, std::move(values));
		}
		else
		{
			sec->second.insert_or_assign(name.value, std::move(text));
		}
		return {};
	}
	[[nodiscard]] auto set_section(section group) -> result<void>
	{
		if (!detail::valid_section(group.value))
		{
			return std::unexpected(
				detail::make_error(error_reason::invalid_section, operation::validate));
		}
		if (m_data.contains(group.value))
		{
			return {};
		}
		if (m_data.size() >= m_options.max_sections)
		{
			return std::unexpected(
				detail::make_error(error_reason::resource_limit, operation::validate));
		}
		m_data.try_emplace(group.value);
		return {};
	}
	[[nodiscard]] auto remove_value(section group, key name) -> bool
	{
		const auto sec = m_data.find(group.value);
		if (sec == m_data.end())
		{
			return false;
		}
		const auto entry = sec->second.find(name.value);
		if (entry == sec->second.end())
		{
			return false;
		}
		sec->second.erase(entry);
		return true;
	}
	[[nodiscard]] auto remove_section(section group) -> bool
	{
		const auto entry = m_data.find(group.value);
		if (entry == m_data.end())
		{
			return false;
		}
		m_data.erase(entry);
		return true;
	}
	[[nodiscard]] auto get_sections() const -> std::vector<std::string>
	{
		std::vector<std::string> result;
		result.reserve(m_data.size());
		for (const auto &[name, ignored] : m_data)
		{
			result.push_back(name);
		}
		return result;
	}
	[[nodiscard]] auto get_keys(section group) const -> std::vector<std::string>
	{
		std::vector<std::string> result;
		if (const auto sec = m_data.find(group.value); sec != m_data.end())
		{
			result.reserve(sec->second.size());
			for (const auto &[name, ignored] : sec->second)
			{
				result.push_back(name);
			}
		}
		return result;
	}
	[[nodiscard]] auto write_stream(std::ostream &stream) const -> result<void>
	{
		auto text = detail::serialize(m_data, m_options);
		if (!text)
		{
			return std::unexpected(std::move(text.error()));
		}
		if (!stream.good())
		{
			return std::unexpected(detail::io_error(operation::write));
		}
		operation action = operation::write;
		try
		{
			auto remaining = std::string_view(*text);
			while (!remaining.empty())
			{
				const auto size = (std::min)(remaining.size(), std::size_t{4096});
				if (stream.rdbuf()->sputn(std::to_address(remaining.begin()),
										  static_cast<std::streamsize>(size)) !=
					static_cast<std::streamsize>(size))
				{
					detail::set_state(stream, std::ios::badbit);
					return std::unexpected(detail::io_error(action));
				}
				remaining.remove_prefix(size);
			}
			action = operation::flush;
			if (stream.rdbuf()->pubsync() != 0)
			{
				detail::set_state(stream, std::ios::badbit);
				return std::unexpected(detail::io_error(action));
			}
		}
		catch (const std::bad_alloc &)
		{
			throw;
		}
		catch (const std::system_error &diagnostic)
		{
			detail::set_state(stream, std::ios::badbit);
			return std::unexpected(detail::io_error(
				action, diagnostic.code() ? diagnostic.code()
										  : std::make_error_code(std::errc::io_error)));
		}
		catch (...)
		{
			detail::set_state(stream, std::ios::badbit);
			return std::unexpected(detail::io_error(action));
		}
		return {};
	}
	[[nodiscard]] auto write_file(const std::filesystem::path &path) const -> result<void>
	{
		auto text = detail::serialize(m_data, m_options);
		if (!text)
		{
			return std::unexpected(std::move(text.error()));
		}
		auto resolved = detail::absolute_path(path, operation::inspect);
		if (!resolved)
		{
			return std::unexpected(std::move(resolved.error()));
		}
		detail::file_ops ops(std::move(*resolved));
		auto outcome = detail::atomic_write(ops, *text);
		if (!outcome)
		{
			outcome.error().path = ops.target;
		}
		return outcome;
	}
	[[nodiscard]] auto write_file() const -> result<void>
	{
		if (!m_path)
		{
			return std::unexpected(
				detail::make_error(error_reason::no_file_path, operation::write));
		}
		return write_file(*m_path);
	}
	friend auto operator>>(std::istream &stream, ini_manager &manager) -> std::istream &
	{
		const auto outcome = manager.load_stream(stream);
		if (!outcome)
		{
			detail::set_state(stream, std::ios::failbit);
		}
		if ((stream.rdstate() & stream.exceptions()) != 0)
		{
			throw std::ios_base::failure("INI extraction failed or reached EOF");
		}
		return stream;
	}
	friend auto operator<<(std::ostream &stream, const ini_manager &manager)
		-> std::ostream &
	{
		const auto outcome = manager.write_stream(stream);
		if (!outcome)
		{
			detail::set_state(stream, std::ios::failbit);
		}
		if ((stream.rdstate() & stream.exceptions()) != 0)
		{
			throw std::ios_base::failure("INI insertion failed");
		}
		return stream;
	}
};
} // namespace ini
#endif
