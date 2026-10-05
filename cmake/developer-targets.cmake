include_guard(GLOBAL)
option(INI_MANAGER_ENABLE_SANITIZERS "Instrument project executables with ASan/UBSan" OFF)
option(ENABLE_COVERAGE "Instrument project executables for coverage" OFF)
option(INI_MANAGER_WARNINGS_AS_ERRORS "Reject compiler and linker warnings in project targets" ON)

function(ini_manager_developer_target target)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_SCAN_FOR_MODULES OFF
        COMPILE_WARNING_AS_ERROR "${INI_MANAGER_WARNINGS_AS_ERRORS}"
        LINK_WARNING_AS_ERROR "${INI_MANAGER_WARNINGS_AS_ERRORS}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()
    if(INI_MANAGER_ENABLE_SANITIZERS)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$" OR MSVC)
            message(FATAL_ERROR "Sanitizers require a GCC/Clang driver with ASan and UBSan")
        endif()
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
    if(ENABLE_COVERAGE)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            message(FATAL_ERROR "The lcov coverage configuration requires GCC")
        endif()
        target_compile_options(${target} PRIVATE --coverage -O0 -g)
        target_link_options(${target} PRIVATE --coverage)
    endif()
endfunction()
