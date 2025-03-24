# C++23 Header-Only INI Manager Library

[![C++ Standard](https://img.shields.io/badge/C++-23-blueviolet.svg)](https://en.cppreference.com/w/cpp/23)

This is a header-only C++23 library for parsing, reading, and writing INI files. It provides a simple and intuitive interface to manage configuration settings within your C++ applications.

## Features

* **Header-Only:** Easy to integrate into your projects by simply including the header file.
* **C++23 Standard:** Leverages modern C++ features like `std::expected` and `std::format` for improved error handling and formatting.
* **INI File Parsing:** Supports standard INI file syntax with sections and key-value pairs.
* **Reading Values:** Provides methods to retrieve values as `std::string`, `int`, `double`, and `bool` with automatic type conversion.
* **Optional Values:** Uses `std::optional` to handle cases where a requested value or section does not exist.
* **Default Values:** Offers a convenient way to get a value or a default if it's not found.
* **Setting Values:** Allows you to set new values or modify existing ones.
* **Creating Sections:** Easily create new sections if they don't already exist.
* **Removing Values and Sections:** Provides functions to remove specific key-value pairs or entire sections.
* **Loading from File:** Reads INI data from a specified file path.
* **Loading from Stream:** Parses INI data from any `std::istream`.
* **Adding from File/Stream:** Merges INI data from a file or stream into an existing `ini_manager` object.
* **Writing to File:** Saves the current configuration to a file.
* **Writing to Stream:** Outputs the configuration data to any `std::ostream`.
* **Stream Operators:** Overloads `operator>>` and `operator<<` for convenient reading from and writing to streams.
* **FetchContent Ready:** Easily integrate into CMake projects using FetchContent.

## Requirements

* A C++23 compliant compiler (e.g., GCC 13+, Clang 16+).

## Installation

As a header-only library, **ini_manager** can be integrated into your projects in a few ways:

### Direct Include:

Simply include the ```ini_manager.hpp``` header file in your C++ project and ensure the directory containing it is in your compiler's include path.

```cpp
#include "ini_manager/ini_manager.hpp"
```

### Using CMake's FetchContent (Recommended for CMake Projects):

If you are using CMake as your build system, you can easily integrate **ini_manager** using the ```FetchContent module```. Add the following to your ```CMakeLists.txt``` file:

```CMake
include(FetchContent)

FetchContent_Declare(
    ini_manager
    GIT_REPOSITORY https://github.com/LineGM/ini_manager.git
    GIT_TAG        <desired_tag_or_branch> # Optional: Specify a tag or branch
)

FetchContent_MakeAvailable(ini_manager)
```

After this, you can link library to your target:

```CMake
target_link_libraries(
    <your_target>
    PRIVATE ini_manager::ini_manager
)
```

Now simply include the ```ini_manager.hpp``` header file in your target source code:

```cpp
#include "ini_manager/ini_manager.hpp"
```

## Usage

Here are some examples of how to use the **ini_manager** library:

### Example 1: Creating and Writing to a File

```cpp
#include "ini_manager/ini_manager.hpp"
#include <iostream>

int main() {
    ini::ini_manager config;
    config.set_section("Section1");
    config.set_value("Section1", "Key1", "Value1");
    config.set_value("Section1", "KeyInt", 42);

    auto write_result = config.write_file("example1.ini");
    if (write_result.has_value()) {
        std::cout << "Configuration written to example1.ini" << std::endl;
    } else {
        std::cerr << "Error writing: " << write_result.error().message() << std::endl;
    }
    return 0;
}
```

### Example 2: Reading and Modifying an Existing File

```cpp
#include "ini_manager/ini_manager.hpp"
#include <iostream>
#include <fstream>

int main() {
    // Create a dummy INI file for this example
    std::ofstream dummy_file("example2.ini");
    dummy_file << "[Settings]\nOption1 = OldValue\nNumber = 100\n";
    dummy_file.close();

    auto config_result = ini::ini_manager::from_file("example2.ini");
    if (config_result.has_value()) {
        auto config = config_result.value();
        if (auto option = config.get_value(ini::section{"Settings"}, ini::key{"Option1"})) {
            std::cout << "Option1: " << *option << std::endl;
        }
        config.set_value("Settings", "Option1", "NewValue");
        config.set_value("Settings", "Number", 200);
        config.write_file("example2.ini");
    } else {
        std::cerr << "Error loading file: " << config_result.error().message() << std::endl;
    }
    return 0;
}
```

### Example 3: Reading from a Stream

```cpp
#include "ini_manager/ini_manager.hpp"
#include <iostream>
#include <sstream>

int main() {
    std::istringstream ini_stream("[Section]\nData = StreamData\nCounter = 5\n");
    auto config_result = ini::ini_manager::from_stream(ini_stream);
    if (config_result.has_value()) {
        auto config = config_result.value();
        if (auto data = config.get_value(ini::section{"Section"}, ini::key{"Data"})) {
            std::cout << "Data: " << *data << std::endl;
        }
        if (auto counter = config.get_value<int>(ini::section{"Section"}, ini::key{"Counter"})) {
            std::cout << "Counter: " << *counter << std::endl;
        }
    } else {
        std::cerr << "Error loading from stream." << std::endl;
    }
    return 0;
}
```

### Example 4: Using Stream Operators

```cpp
#include "ini_manager/ini_manager.hpp"
#include <iostream>
#include <sstream>

int main() {
    std::istringstream ini_stream("[Section]\nValue = FromStream\n");
    ini::ini_manager config;
    ini_stream >> config;
    std::cout << config << std::endl; // Write the loaded config to stdout
    return 0;
}
```

For more detailed examples, please refer to the .cpp files provided in the ```example/``` directory.

## API Overview

### ```ini::section``` and ```ini::key```

Simple structs to represent ```section``` and ```key``` names as ```std::string_view```.

### ```ini::ini_manager```
* ```ini_manager()```: Default constructor to create an empty configuration.
* ```static auto from_file(const std::string &file_path) -> std::expected<ini_manager, std::error_code>```: Static factory function to load configuration from a file.
* ```static auto from_stream(std::istream &istream) -> std::expected<ini_manager, std::error_code>```: Static factory function to load configuration from a stream.
* ```auto operator(std::string_view section) -> section_accessor```: Accessor for modifying values within a section.
* ```auto operator(std::string_view section) const -> const_section_accessor```: Accessor for reading values within a section.
* ```auto get_value(section section, key key) const noexcept -> std::optional<std::string>```: Retrieves a string value.
* ```template <typename T> auto get_value(section section, key key) const noexcept -> std::optional<T>```: Retrieves a value with automatic type conversion.
* ```auto get_value_or_default(section section, key key, std::string default_value) const noexcept -> std::string```: Retrieves a string value or a default if not found.
* ```template <typename T> auto get_value_or_default(section section, key key, T default_value) const noexcept -> T```: Retrieves a value with type conversion or a default if not found.
* ```template <typename T> requires std::formattable<T, char> void set_value(std::string_view section, std::string_view key, T value) noexcept```: Sets a value for a given section and key.
* ```void set_section(const std::string &section) noexcept```: Creates a new section if it doesn't exist.
* ```auto remove_value(section section, key key) noexcept -> bool```: Removes a key-value pair.
* ```auto remove_section(section section) noexcept -> bool```: Removes an entire section.
* ```auto load_file(const std::string &file_path) -> std::expected<void, std::error_code>```: Loads configuration from a file, overwriting existing data.
* ```auto load_stream(std::istream &istream) -> std::expected<void, std::error_code>```: Loads configuration from a stream, overwriting existing data.
* ```auto add_from_stream(std::istream &istream) -> std::expected<void, std::error_code>```: Adds configuration data from a stream, merging with existing data.
* ```auto add_from_file(const std::string &file_path) -> std::expected<void, std::error_code>```: Adds configuration data from a file, merging with existing data.
* ```auto write_file(const std::string &file_path) const -> std::expected<void, std::error_code>```: Writes the configuration to a file.
* ```auto write_file() const -> std::expected<void, std::error_code>```: Writes the configuration to the file specified during loading (if any).
* ```friend auto operator<<(std::ostream &ostream, const ini_manager &manager) -> std::ostream &```: Writes the configuration to an output stream.
* ```friend auto operator>>(std::istream &istream, ini_manager &manager) -> std::istream &```: Reads the configuration from an input stream.

### Nested Classes
* ```section_accessor```: Provides non-const access to keys within a section using operator, returning a ```std::string&```.
* ```const_section_accessor```: Provides const access to keys within a section using operator, returning a ```std::optional<std::string>```.

## Building
For information on building, please refer to the [**BUILDING**](BUILDING.md) file.

## Contributing
Contributions are welcome! Please read the [**CONTRIBUTING**](CONTRIBUTING.md) file for guidelines on how to contribute to this project.

## License
This library is licensed under the [**LICENSE**](LICENSE).

## Code of Conduct
Please adhere to the [**CODE_OF_CONDUCT**](CODE_OF_CONDUCT.md) when participating in this project.

## Hacking
For more in-depth information about the library's internals and development, please see the [**HACKING**](HACKING.md) file.
