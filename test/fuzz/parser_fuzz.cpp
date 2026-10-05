#include <bit>
#include <cstdint>
#include <cstdlib>
#include <ini_manager/ini_manager.hpp>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
// Required external symbol name from the libFuzzer ABI.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) -> int
{
	if (size == 0)
	{
		return 0;
	}
	constexpr ini::parse_options limits{.max_line_bytes = 4096,
										.max_input_bytes = 65536,
										.max_sections = 64,
										.max_keys = 512};
	auto options = limits;
	const auto bytes = std::span(data, size);
	const auto flags = bytes.front();
	options.allow_colon = (flags & 1U) != 0;
	options.inline_comments = (flags & 2U) != 0;
	options.duplicates = (flags & 4U) != 0 ? ini::duplicate_policy::last_wins
										   : ini::duplicate_policy::reject;
	const auto chars =
		bytes.subspan(1) | std::views::transform([](std::uint8_t byte) -> char {
			return std::bit_cast<char>(byte);
		});
	std::istringstream input(std::string(std::from_range, chars));
	auto parsed = ini::ini_manager::from_stream(input, options);
	if (!parsed)
	{
		return 0;
	}
	std::ostringstream output;
	if (!parsed->write_stream(output))
	{
		return 0; // canonical spelling may exceed input limits
	}
	std::istringstream again(output.str());
	auto reread = ini::ini_manager::from_stream(again, options);
	if (!reread || parsed->get_sections() != reread->get_sections())
	{
		std::abort();
	}
	for (const auto &group : parsed->get_sections())
	{
		if (parsed->get_keys({group}) != reread->get_keys({group}))
		{
			std::abort();
		}
		for (const auto &name : parsed->get_keys({group}))
		{
			if (parsed->get_value({group}, {name}).value() !=
				reread->get_value({group}, {name}).value())
			{
				std::abort();
			}
		}
	}
	return 0;
}
