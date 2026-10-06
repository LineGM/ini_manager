#include "ini_manager/ini_manager.hpp"
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
constexpr std::size_t payload_size = 4096;

void require(ini::result<void> outcome)
{
	if (!outcome)
	{
		throw std::runtime_error(outcome.error().message());
	}
}

template <class T> auto take(ini::result<T> outcome) -> T
{
	if (!outcome)
	{
		throw std::runtime_error(outcome.error().message());
	}
	return std::move(*outcome);
}

struct counted_text
{
	std::size_t size = 0;
	friend auto operator>>(std::istream &input, counted_text &result) -> std::istream &
	{
		result.size = static_cast<std::size_t>(std::distance(
			std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()));
		return input;
	}
};

auto set_strings(std::size_t iterations) -> std::size_t
{
	ini::ini_manager config;
	for (std::size_t index = 0; index < iterations; ++index)
	{
		require(config.set_value({"s"}, {"k"}, std::string(payload_size, 'x')));
	}
	return take(config.get_value({"s"}, {"k"})).size();
}

auto string_defaults(std::size_t iterations) -> std::size_t
{
	const ini::ini_manager config;
	std::size_t total = 0;
	for (std::size_t index = 0; index < iterations; ++index)
	{
		total += take(config.get_value_or_default({"s"}, {"k"},
												  std::string(payload_size, 'x')))
					 .size();
	}
	return total;
}

auto custom_reads(std::size_t iterations) -> std::size_t
{
	ini::ini_manager config;
	require(config.set_value({"s"}, {"k"}, std::string(payload_size, 'x')));
	std::size_t total = 0;
	for (std::size_t index = 0; index < iterations; ++index)
	{
		total += take(config.get_value<counted_text>({"s"}, {"k"})).size;
	}
	return total;
}

auto insert_sections(std::size_t iterations) -> std::size_t
{
	ini::ini_manager config;
	for (std::size_t index = 0; index < iterations; ++index)
	{
		require(config.set_value({std::to_string(index)}, {"k"}, "x"));
	}
	return config.get_sections().size();
}

auto large_config() -> ini::ini_manager
{
	ini::ini_manager config;
	constexpr std::size_t keys = 100;
	for (std::size_t index = 0; index < keys; ++index)
	{
		require(config.set_value({"s"}, {std::to_string(index)},
								 std::string(payload_size, 'x')));
	}
	return config;
}

auto merge_documents(std::size_t iterations) -> std::size_t
{
	auto config = large_config();
	for (std::size_t index = 0; index < iterations; ++index)
	{
		std::istringstream input("[s]\nextra=value\n");
		require(config.add_from_stream(input));
	}
	return config.get_keys({"s"}).size();
}

auto serialize_documents(std::size_t iterations) -> std::size_t
{
	const auto config = large_config();
	std::size_t total = 0;
	for (std::size_t index = 0; index < iterations; ++index)
	{
		std::ostringstream output;
		require(config.write_stream(output));
		total += output.view().size();
	}
	return total;
}

struct workload
{
	std::string_view name;
	std::size_t (*run)(std::size_t);
};

auto run(std::span<const char *const> arguments) -> int
{
	if (arguments.size() != 3)
	{
		throw std::runtime_error("Expected a workload name and positive iteration count");
	}
	const std::string_view count(arguments.at(2));
	std::size_t iterations = 0;
	const auto parsed = std::from_chars(std::to_address(count.begin()),
										std::to_address(count.end()), iterations);
	if (!parsed || parsed.ptr != std::to_address(count.end()) || iterations == 0)
	{
		throw std::runtime_error("Invalid iteration count");
	}
	constexpr std::array workloads{
		workload{"set_strings", set_strings},
		workload{"string_defaults", string_defaults},
		workload{"custom_reads", custom_reads},
		workload{"insert_sections", insert_sections},
		workload{"merge_documents", merge_documents},
		workload{"serialize_documents", serialize_documents},
	};
	for (const auto &[name, execute] : workloads)
	{
		if (name == arguments.at(1))
		{
			const auto start = std::chrono::steady_clock::now();
			const auto checksum = execute(iterations);
			const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
									 std::chrono::steady_clock::now() - start)
									 .count();
			std::cout << "{\"elapsed_ns\":" << elapsed << ",\"checksum\":" << checksum
					  << "}\n";
			return EXIT_SUCCESS;
		}
	}
	throw std::runtime_error("Unknown workload");
}
} // namespace

auto main(int argc, const char *argv[]) -> int
{
	try
	{
		return run({argv, static_cast<std::size_t>(argc)});
	}
	catch (const std::exception &failure)
	{
		std::cerr << failure.what() << '\n';
		return EXIT_FAILURE;
	}
}
