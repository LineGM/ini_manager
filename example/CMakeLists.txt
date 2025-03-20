cmake_minimum_required(VERSION 3.14)

project(ini_managerExamples CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

if(PROJECT_IS_TOP_LEVEL)
	find_package(ini_manager REQUIRED)
endif()

add_custom_target(run-examples)

function(add_example NAME)
	add_executable(
		"${NAME}"
		"${NAME}.cpp"
	)

	target_link_libraries(
		"${NAME}"
		PRIVATE ini_manager::ini_manager
	)

	target_compile_features(
		"${NAME}"
		PRIVATE cxx_std_23
	)

	add_custom_target(
		"run_${NAME}"
		COMMAND "${NAME}" VERBATIM
	)

	add_dependencies(
		"run_${NAME}"
		"${NAME}"
	)

	add_dependencies(
		run-examples
		"run_${NAME}"
	)
endfunction()

add_example(1_empty_example)
add_example(2_path_example)
add_example(3_stream_example)
add_example(4_load_file_example)
add_example(5_add_from_stream_example)
add_example(6_load_stream_example)
add_example(7_add_from_file_example)
add_example(8_operator_example)

add_folders(Example)
