include(CheckCXXSourceCompiles)
# Isolated scope: never change compiler flags in the parent project.
function(ini_manager_check_features)
    set(CMAKE_CXX_STANDARD 26)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_REQUIRED_INCLUDES "${PROJECT_SOURCE_DIR}/include")
    check_cxx_source_compiles([=[
#include <ini_manager/ini_manager.hpp>
int main() {
    ini::ini_manager config;
    auto written = config.set_value({"numbers"}, {"value"}, 1.25L);
    auto read = config.get_value<long double>({"numbers"}, {"value"});
    return !written || !read;
}
]=] INI_MANAGER_HAS_CXX26_LIBRARY)
    if(NOT INI_MANAGER_HAS_CXX26_LIBRARY)
        message(FATAL_ERROR "ini_manager requires C++26 heterogeneous map insertion, charconv result testing, expected, format and floating-point charconv (including long double). Select a supported compiler AND standard library; see README.md.")
    endif()
endfunction()
ini_manager_check_features()
