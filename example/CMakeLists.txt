cmake_minimum_required(VERSION 4.0...4.4)

project(ini_managerExamples CXX)

include(../cmake/folders.cmake)

if(PROJECT_IS_TOP_LEVEL)
	find_package(ini_manager 1.1 REQUIRED)
    include(../cmake/developer-targets.cmake)
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

    ini_manager_developer_target(${NAME})

	add_custom_target(
		"run_${NAME}"
		COMMAND $<TARGET_FILE:${NAME}> VERBATIM
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
