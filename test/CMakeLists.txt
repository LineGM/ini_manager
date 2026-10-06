cmake_minimum_required(VERSION 4.0...4.4)
project(ini_managerTests LANGUAGES CXX)
if(PROJECT_IS_TOP_LEVEL)
    find_package(ini_manager 1.0 REQUIRED)
    include(../cmake/developer-targets.cmake)
    enable_testing()
endif()
include(FetchContent)
FetchContent_Declare(boost.ut
    GIT_REPOSITORY https://github.com/boost-ext/ut.git
    # Includes the Darwin constructor-priority fix; pin the reviewed source.
    GIT_TAG bfbec2a580c21fb3ffa328ede8821243e7cdf247
    SYSTEM)
set(BOOST_UT_DISABLE_MODULE ON CACHE BOOL "Disable optional Boost.UT module")
FetchContent_MakeAvailable(boost.ut)
foreach(name IN ITEMS ini_manager parser io)
    add_executable(${name}_test source/${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE ini_manager::ini_manager Boost::ut)
    ini_manager_developer_target(${name}_test)
    add_test(NAME ${name}_test COMMAND ${name}_test)
    set_tests_properties(${name}_test PROPERTIES LABELS "unit;${name}")
endforeach()
add_executable(header_test source/header_a.cpp source/header_b.cpp)
target_link_libraries(header_test PRIVATE ini_manager::ini_manager)
ini_manager_developer_target(header_test)
add_test(NAME header_test COMMAND header_test)
set_tests_properties(header_test PROPERTIES LABELS "integration;headers")
if(NOT PROJECT_IS_TOP_LEVEL)
    add_test(NAME consumer_test COMMAND ${CMAKE_COMMAND}
        "-DINI_SOURCE=${ini_manager_SOURCE_DIR}" "-DINI_BINARY=${ini_manager_BINARY_DIR}"
        "-DINI_COMPILER=${CMAKE_CXX_COMPILER}" "-DINI_FLAGS=${CMAKE_CXX_FLAGS}"
        "-DINI_CONFIG=$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/consumer/check.cmake")
    set_tests_properties(consumer_test PROPERTIES LABELS "integration;package" RUN_SERIAL TRUE)
endif()
