set(
	FORMAT_PATTERNS
	example/*.cpp example/*.hpp
	include/*.hpp
	test/*.cpp test/*.hpp
	CACHE STRING
	"; separated patterns relative to the project source dir to format"
)

set(FORMAT_COMMAND clang-format CACHE STRING "Formatter to use")

add_custom_target(
	format-check
	COMMAND "${CMAKE_COMMAND}"
	-D "FORMAT_COMMAND=${FORMAT_COMMAND}"
	-D "PATTERNS=${FORMAT_PATTERNS}"
	-P "${PROJECT_SOURCE_DIR}/cmake/lint.cmake"
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	COMMENT "Linting the code"
	VERBATIM
)

add_custom_target(
	format-fix
	COMMAND "${CMAKE_COMMAND}"
	-D "FORMAT_COMMAND=${FORMAT_COMMAND}"
	-D "PATTERNS=${FORMAT_PATTERNS}"
	-D FIX=YES
	-P "${PROJECT_SOURCE_DIR}/cmake/lint.cmake"
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	COMMENT "Fixing the code"
	VERBATIM
)

set(TIDY_COMMAND clang-tidy CACHE STRING "Static analyzer to use")
add_custom_target(tidy-check
    COMMAND "${CMAKE_COMMAND}"
        "-DTIDY_COMMAND=${TIDY_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DBINARY_DIR=${PROJECT_BINARY_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/tidy.cmake"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL
    COMMENT "Checking project code with .clang-tidy"
    VERBATIM)
