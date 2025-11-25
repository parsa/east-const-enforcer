include_guard(GLOBAL)

function(_east_const_list_to_json OUTPUT_VAR)
  set(_json "")
  foreach(_item IN LISTS ARGN)
    string(REPLACE "\\" "\\\\" _escaped "${_item}")
    string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
    if(NOT _json STREQUAL "")
      string(APPEND _json ", ")
    endif()
    string(APPEND _json "\"${_escaped}\"")
  endforeach()
  set(${OUTPUT_VAR} "${_json}" PARENT_SCOPE)
endfunction()

function(_east_const_escape_json_string OUTPUT_VAR INPUT_STRING)
  string(REPLACE "\\" "\\\\" _escaped "${INPUT_STRING}")
  string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
  set(${OUTPUT_VAR} "\"${_escaped}\"" PARENT_SCOPE)
endfunction()

function(add_east_const_integration_tests)
  set(options)
  set(oneValueArgs BUILD_DIR SCRIPT PYTHON_EXECUTABLE CLANG_TIDY_PATH)
  set(multiValueArgs)
  cmake_parse_arguments(H "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT H_BUILD_DIR)
    message(FATAL_ERROR "add_east_const_integration_tests requires BUILD_DIR")
  endif()
  if(NOT H_SCRIPT)
    message(FATAL_ERROR "add_east_const_integration_tests requires SCRIPT")
  endif()
  if(NOT H_PYTHON_EXECUTABLE)
    message(FATAL_ERROR "add_east_const_integration_tests requires PYTHON_EXECUTABLE")
  endif()
  if(NOT H_CLANG_TIDY_PATH)
    message(FATAL_ERROR "add_east_const_integration_tests requires CLANG_TIDY_PATH")
  endif()

  set(EAST_CONST_INTEGRATION_EXTRA_INCLUDE_DIRS "" CACHE STRING
      "Semicolon-separated include directories appended (via -isystem) for integration compile flags")
  set(EAST_CONST_INTEGRATION_EXTRA_FLAGS "" CACHE STRING
      "Additional compile flags appended after default integration flags")
  set(EAST_CONST_INTEGRATION_DEFAULT_TOOL_ARGS "" CACHE STRING
      "Default arguments passed to the standalone tool for integration cases")
  set(EAST_CONST_INTEGRATION_DEFAULT_TIDY_ARGS "" CACHE STRING
      "Default arguments passed to clang-tidy for integration cases")

  set(EAST_CONST_INTEGRATION_COMPILE_FLAGS)
  if(CMAKE_CXX_STANDARD)
    list(APPEND EAST_CONST_INTEGRATION_COMPILE_FLAGS "-std=c++${CMAKE_CXX_STANDARD}")
  else()
    list(APPEND EAST_CONST_INTEGRATION_COMPILE_FLAGS "-std=c++17")
  endif()

  set(_implicit_include_dirs ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
  list(REMOVE_ITEM _implicit_include_dirs "")
  list(REMOVE_DUPLICATES _implicit_include_dirs)
  foreach(_dir IN LISTS _implicit_include_dirs)
    if(EXISTS "${_dir}")
      list(APPEND EAST_CONST_INTEGRATION_COMPILE_FLAGS "-isystem" "${_dir}")
    endif()
  endforeach()

  foreach(_extra_dir IN LISTS EAST_CONST_INTEGRATION_EXTRA_INCLUDE_DIRS)
    if(NOT _extra_dir STREQUAL "")
      list(APPEND EAST_CONST_INTEGRATION_COMPILE_FLAGS "-isystem" "${_extra_dir}")
    endif()
  endforeach()

  if(EAST_CONST_INTEGRATION_EXTRA_FLAGS)
    list(APPEND EAST_CONST_INTEGRATION_COMPILE_FLAGS ${EAST_CONST_INTEGRATION_EXTRA_FLAGS})
  endif()

  _east_const_list_to_json(EAST_CONST_COMPILE_FLAGS_JSON ${EAST_CONST_INTEGRATION_COMPILE_FLAGS})
  _east_const_list_to_json(EAST_CONST_TOOL_ARGS_JSON ${EAST_CONST_INTEGRATION_DEFAULT_TOOL_ARGS})
  _east_const_list_to_json(EAST_CONST_TIDY_ARGS_JSON ${EAST_CONST_INTEGRATION_DEFAULT_TIDY_ARGS})
  _east_const_escape_json_string(EAST_CONST_CLANG_TIDY_PATH_JSON "${H_CLANG_TIDY_PATH}")

  set(EAST_CONST_INTEGRATION_CONFIG "${H_BUILD_DIR}/tests/integration/config.json")
  file(MAKE_DIRECTORY "${H_BUILD_DIR}/tests/integration")
  configure_file(
    ${CMAKE_SOURCE_DIR}/tests/integration/config.json.in
    ${EAST_CONST_INTEGRATION_CONFIG}
    @ONLY)

  add_custom_target(east_const_integration_config DEPENDS "${EAST_CONST_INTEGRATION_CONFIG}")

  execute_process(
    COMMAND ${H_PYTHON_EXECUTABLE} ${H_SCRIPT} --list-cases
    OUTPUT_VARIABLE EAST_CONST_INTEGRATION_CASES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE EAST_CONST_INTEGRATION_LIST_RESULT)

  if(NOT EAST_CONST_INTEGRATION_LIST_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to enumerate integration cases for CTest")
  endif()

  string(REPLACE "\r" "\n" EAST_CONST_INTEGRATION_CASES "${EAST_CONST_INTEGRATION_CASES}")
  string(REGEX REPLACE "\n" ";" EAST_CONST_INTEGRATION_CASE_LIST "${EAST_CONST_INTEGRATION_CASES}")
  list(REMOVE_ITEM EAST_CONST_INTEGRATION_CASE_LIST "")

  foreach(EAST_CONST_CASE ${EAST_CONST_INTEGRATION_CASE_LIST})
    string(REGEX REPLACE "[^A-Za-z0-9_-]" "_" EAST_CONST_CASE_NAME "${EAST_CONST_CASE}")
    add_test(
      NAME "east-const-integration-${EAST_CONST_CASE_NAME}"
      COMMAND ${H_PYTHON_EXECUTABLE}
              ${H_SCRIPT}
              --build-dir ${H_BUILD_DIR}
              --config ${EAST_CONST_INTEGRATION_CONFIG}
              --clang-tidy ${H_CLANG_TIDY_PATH}
              --case ${EAST_CONST_CASE})
  endforeach()

  add_test(
    NAME east-const-integration
    COMMAND ${H_PYTHON_EXECUTABLE}
            ${H_SCRIPT}
            --build-dir ${H_BUILD_DIR}
            --config ${EAST_CONST_INTEGRATION_CONFIG}
            --clang-tidy ${H_CLANG_TIDY_PATH}
  )
endfunction()
