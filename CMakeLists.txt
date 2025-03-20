cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
	ini_manager
	VERSION 0.1.0
	DESCRIPTION "Проект для работы с .ini файлами."
	HOMEPAGE_URL "http://kz-git-001.adm.local/processing_department/crossplatform_development/ini_manager"
	LANGUAGES NONE
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
	ini_manager_ini_manager INTERFACE
)

add_library(
	ini_manager::ini_manager
	ALIAS ini_manager_ini_manager
)

set_property(
	TARGET ini_manager_ini_manager PROPERTY
	EXPORT_NAME ini_manager
)

target_include_directories(
	ini_manager_ini_manager ${warning_guard}
	INTERFACE "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(
	ini_manager_ini_manager
	INTERFACE cxx_std_23
)

# Copy compile_commands.json from current build directory to build/
# (for clangd extension)
execute_process(
	COMMAND ${CMAKE_COMMAND} -E create_symlink
		"${CMAKE_BINARY_DIR}/compile_commands.json"
		"${CMAKE_BINARY_DIR}/../compile_commands.json"
)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
	include(cmake/install-rules.cmake)
endif()

# ---- Examples ----

if(PROJECT_IS_TOP_LEVEL)
	option(BUILD_EXAMPLES "Build examples tree." "${ini_manager_DEVELOPER_MODE}")
	if(BUILD_EXAMPLES)
		add_subdirectory(example)
	endif()
endif()

# ---- Developer mode ----

if(NOT ini_manager_DEVELOPER_MODE)
	return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
	message(
		AUTHOR_WARNING
		"Developer mode is intended for developers of ini_manager"
	)
endif()

include(cmake/dev-mode.cmake)
