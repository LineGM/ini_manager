cmake_minimum_required(VERSION 3.14)

project(ini_managerTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
	find_package(ini_manager REQUIRED)
	enable_testing()
endif()

include(FetchContent)

set(BOOST.UT_VERSION_DOWNLOAD "2.3.0")
FetchContent_Declare(
	boost.ut
	GIT_REPOSITORY https://github.com/boost-ext/ut.git
	GIT_TAG v${BOOST.UT_VERSION_DOWNLOAD}
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/_deps_src/boost.ut-${BOOST.UT_VERSION_DOWNLOAD}
)

FetchContent_MakeAvailable(boost.ut)

# ---- Tests ----

add_executable(
	ini_manager_test
	source/ini_manager_test.cpp
)

target_link_libraries(
	ini_manager_test
	PRIVATE ini_manager::ini_manager
	PRIVATE Boost::ut
)

target_compile_features(
	ini_manager_test
	PRIVATE cxx_std_23
)

add_test(
	NAME ini_manager_test
	COMMAND ini_manager_test
)

# ---- End-of-file commands ----

add_folders(Test)
