# Building and installing ini_manager 1.1.0

Use CMake 4.0+ and a C++26 compiler and standard library meeting the requirements
in [README.md](README.md). Consuming the library requires only its public header
and the platform's standard and system libraries.

## Install a package

```sh
cmake --preset=release
cmake --build --preset=release
cmake --install build/release --prefix /desired/prefix
```

With a multi-configuration generator, pass `--config Release` to build/install.
The package provides the target `ini_manager::ini_manager`:

```cmake
cmake_minimum_required(VERSION 4.0)
project(my_app LANGUAGES CXX)
find_package(ini_manager 1.1 CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE ini_manager::ini_manager)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix when it is outside CMake's
search paths. Package version matching accepts compatible releases within the
requested major version. The exported target carries the C++26 requirement and
header include directories. A relocated installation remains usable.

## Include source directly

```cmake
add_subdirectory(path/to/ini_manager)
target_link_libraries(my_app PRIVATE ini_manager::ini_manager)
```

A source checkout can also be made available through FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(ini_manager
    SOURCE_DIR /path/to/ini_manager)
FetchContent_MakeAvailable(ini_manager)
target_link_libraries(my_app PRIVATE ini_manager::ini_manager)
```

For a remote checkout use the repository URL with `GIT_REPOSITORY` and pin
`GIT_TAG` to a reviewed commit. Consumer builds do not create developer targets,
download test dependencies or apply project instrumentation flags. Headers are
treated as system includes when embedded; set `ini_manager_INCLUDES_WITH_SYSTEM`
to `OFF` to inspect dependency warnings in your build.

## Create distributable archives

```sh
cmake --workflow --preset=package-release
```

The TGZ and ZIP archives under `build/release/packages` contain the public header
and relocatable CMake package files. Installation uses the target's `HEADERS`
file set, so the installed files and the exported interface describe the same API.

See [HACKING.md](HACKING.md) for tests, analysis, examples and documentation.
Standalone `example` and `test` CMake projects can use an installed package via
`CMAKE_PREFIX_PATH`.
