include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
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
        message(FATAL_ERROR "ini_manager requires C++26 heterogeneous map insertion, charconv result testing, filesystem path formatting, span streams, expected, format and floating-point charconv (including long double). Select a supported compiler AND standard library; see README.md.")
    endif()
    if(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
        check_cxx_source_runs([=[
#include <ini_manager/ini_manager.hpp>
#include <cmath>
#include <limits>

template<class T> bool roundtrip() {
    ini::ini_manager config;
    for (T value : {T{0}, -T{0}, T{1.25}, T{1} + std::numeric_limits<T>::epsilon(),
                    std::numeric_limits<T>::max(), std::numeric_limits<T>::min(),
                    std::numeric_limits<T>::denorm_min()}) {
        if (!config.set_value({"numbers"}, {"value"}, value)) return false;
        auto read = config.get_value<T>({"numbers"}, {"value"});
        if (!read || *read != value || std::signbit(*read) != std::signbit(value))
            return false;
    }
    return true;
}
int main() {
    return !(roundtrip<float>() && roundtrip<double>() && roundtrip<long double>());
}
]=] INI_MANAGER_HAS_NUMERIC_CONTRACT)
        if(NOT INI_MANAGER_HAS_NUMERIC_CONTRACT)
            message(FATAL_ERROR "The standard library fails the numeric round-trip contract (precision, range, subnormals or signed zero). A declared long double charconv overload is insufficient. This toolchain is unsupported; see README.md.")
        endif()
    endif()
endfunction()
ini_manager_check_features()
