cmake_minimum_required(VERSION 3.21)

# ---- Process project() call in CMakeLists.txt ----

file(READ "${PROJECT_SOURCE_DIR}/CMakeLists.txt" content)

string(FIND "${content}" "project(" index)
if(index EQUAL "-1")
  message(FATAL_ERROR "Could not find \"project(\"")
endif()
string(SUBSTRING "${content}" "${index}" -1 content)

string(FIND "${content}" "\n)\n" index)
if(index EQUAL "-1")
  message(FATAL_ERROR "Could not find \"\\n)\\n\"")
endif()
string(SUBSTRING "${content}" 0 "${index}" content)

file(WRITE "${PROJECT_BINARY_DIR}/docs-ci.project.cmake" "docs_${content}\n)\n")

macro(list_pop_front list out)
  list(GET "${list}" 0 "${out}")
  list(REMOVE_AT "${list}" 0)
endmacro()

function(docs_project name)
  cmake_parse_arguments(PARSE_ARGV 1 "" "" "VERSION;DESCRIPTION;HOMEPAGE_URL" LANGUAGES)
  set(PROJECT_NAME "${name}" PARENT_SCOPE)
  if(DEFINED _VERSION)
    set(PROJECT_VERSION "${_VERSION}" PARENT_SCOPE)
    string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" versions "${_VERSION}")
    string(REPLACE . ";" versions "${versions}")
    set(suffixes MAJOR MINOR PATCH TWEAK)
    while(NOT versions STREQUAL "" AND NOT suffixes STREQUAL "")
      list_pop_front(versions version)
      list_pop_front(suffixes suffix)
      set("PROJECT_VERSION_${suffix}" "${version}" PARENT_SCOPE)
    endwhile()
  endif()
  if(DEFINED _DESCRIPTION)
    set(PROJECT_DESCRIPTION "${_DESCRIPTION}" PARENT_SCOPE)
  endif()
  if(DEFINED _HOMEPAGE_URL)
    set(PROJECT_HOMEPAGE_URL "${_HOMEPAGE_URL}" PARENT_SCOPE)
  endif()
endfunction()

include("${PROJECT_BINARY_DIR}/docs-ci.project.cmake")

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
