# ---- Developer mode ----

# Developer mode enables targets and code paths in the CMake scripts that are
# only relevant for the developer(s) of ini_manager
# Targets necessary to build the project must be provided unconditionally, so
# consumers can trivially build and package the project
if(PROJECT_IS_TOP_LEVEL)
	option(ini_manager_DEVELOPER_MODE "Enable developer mode" OFF)
endif()

# Consumers may explicitly opt out of treating this dependency as a system include.
if(NOT PROJECT_IS_TOP_LEVEL)
    option(ini_manager_INCLUDES_WITH_SYSTEM "Treat ini_manager headers as system includes" ON)
    mark_as_advanced(ini_manager_INCLUDES_WITH_SYSTEM)
endif()
