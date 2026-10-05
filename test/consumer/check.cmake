cmake_minimum_required(VERSION 4.0...4.4)
# A failed command terminates this integration script immediately (CMake 4).
set(CMAKE_EXECUTE_PROCESS_COMMAND_ERROR_IS_FATAL ANY)
function(run)
    execute_process(COMMAND ${ARGV})
endfunction()
if(NOT INI_CONFIG)
    set(INI_CONFIG Release)
endif()
set(root "${INI_BINARY}/consumer-check-1.0.0")
run("${CMAKE_COMMAND}" --install "${INI_BINARY}" --prefix "${root}/prefix" --config "${INI_CONFIG}")
# Relocate a complete installed tree; exported paths must remain prefix-relative.
file(REMOVE_RECURSE "${root}/relocated")
file(RENAME "${root}/prefix" "${root}/relocated")
foreach(mode IN ITEMS subdirectory fetch installed)
    run("${CMAKE_COMMAND}" -S "${INI_SOURCE}/test/consumer" -B "${root}/${mode}"
        "-DMODE=${mode}" "-DINI_SOURCE=${INI_SOURCE}" "-DCMAKE_PREFIX_PATH=${root}/relocated"
        "-DCMAKE_CXX_COMPILER=${INI_COMPILER}" "-DCMAKE_CXX_FLAGS=${INI_FLAGS}")
    run("${CMAKE_COMMAND}" --build "${root}/${mode}" --config "${INI_CONFIG}")
    run("${CMAKE_CTEST_COMMAND}" --test-dir "${root}/${mode}" -C "${INI_CONFIG}" --output-on-failure)
    if(EXISTS "${root}/compile_commands.json" OR IS_SYMLINK "${root}/compile_commands.json")
        message(FATAL_ERROR "Consumer build created unexpected compilation database link")
    endif()
endforeach()
