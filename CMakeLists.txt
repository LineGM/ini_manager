cmake_minimum_required(VERSION 4.0...4.4)
include(cmake/prelude.cmake)
project(
    ini_manager
    VERSION 1.1.0
    DESCRIPTION "Strict header-only C++26 INI configuration library"
    HOMEPAGE_URL "https://github.com/LineGM/ini_manager"
    LANGUAGES CXX
)
include(cmake/variables.cmake)

add_library(ini_manager_ini_manager INTERFACE)
add_library(ini_manager::ini_manager ALIAS ini_manager_ini_manager)
set_property(TARGET ini_manager_ini_manager PROPERTY EXPORT_NAME ini_manager)
target_sources(ini_manager_ini_manager INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES "${PROJECT_SOURCE_DIR}/include/ini_manager/ini_manager.hpp")
if(ini_manager_INCLUDES_WITH_SYSTEM)
    set_property(TARGET ini_manager_ini_manager APPEND PROPERTY
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
endif()
target_compile_features(ini_manager_ini_manager INTERFACE cxx_std_26)

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(cmake/install-rules.cmake)
endif()

# Consumer builds never enable developer targets or change compiler flags.
if(PROJECT_IS_TOP_LEVEL AND ini_manager_DEVELOPER_MODE)
    # No modules are built; avoid generated module response files in clang-tidy.
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    include(cmake/check-features.cmake)
    include(cmake/developer-targets.cmake)
    set_property(TARGET ini_manager_ini_manager PROPERTY VERIFY_INTERFACE_HEADER_SETS ON)
    option(BUILD_EXAMPLES "Build examples" ON)
    option(INI_MANAGER_BUILD_FUZZER "Build Clang libFuzzer parser target" OFF)
    option(INI_MANAGER_BUILD_BENCHMARKS "Build public API measurement workloads" OFF)
    if(CMAKE_EXPORT_COMPILE_COMMANDS AND NOT WIN32)
        # Do not replace a file or link owned by the developer.
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/compile_commands.json" AND
           NOT IS_SYMLINK "${PROJECT_SOURCE_DIR}/compile_commands.json")
            file(CREATE_LINK "${PROJECT_BINARY_DIR}/compile_commands.json"
                "${PROJECT_SOURCE_DIR}/compile_commands.json" SYMBOLIC RESULT link_result)
        endif()
    endif()
    if(BUILD_EXAMPLES)
        add_subdirectory(example)
    endif()
    include(cmake/dev-mode.cmake)
    if(INI_MANAGER_BUILD_BENCHMARKS)
        add_executable(ini_manager_benchmark test/benchmark/workloads.cpp)
        target_link_libraries(ini_manager_benchmark PRIVATE ini_manager::ini_manager)
        ini_manager_developer_target(ini_manager_benchmark)
    endif()
    if(INI_MANAGER_BUILD_FUZZER)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            message(FATAL_ERROR "INI_MANAGER_BUILD_FUZZER requires Clang with libFuzzer")
        endif()
        add_executable(ini_manager_fuzz test/fuzz/parser_fuzz.cpp)
        target_link_libraries(ini_manager_fuzz PRIVATE ini_manager::ini_manager)
        ini_manager_developer_target(ini_manager_fuzz)
        target_compile_options(ini_manager_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
        target_link_options(ini_manager_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()
endif()
