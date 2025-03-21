cmake_minimum_required(VERSION 3.21)

# ---- Generate docs ----

set(DOXYGEN_IN ${PROJECT_SOURCE_DIR}/docs/Doxyfile.in)
set(DOXYGEN_OUT ${PROJECT_SOURCE_DIR}/docs/Doxyfile)
set(working_dir "${PROJECT_SOURCE_DIR}/docs")

configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

file(REMOVE_RECURSE "${working_dir}/html" "${working_dir}/xml")

execute_process(
    COMMAND doxygen ${DOXYGEN_OUT}
    WORKING_DIRECTORY "${working_dir}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL "0")
  message(FATAL_ERROR "doxygen returned with ${result}")
endif()
