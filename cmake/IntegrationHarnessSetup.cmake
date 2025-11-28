# Integration harness configuration helpers.
# Keeps the main CMakeLists clean by moving the JSON/config/test plumbing here.

include_guard(GLOBAL)

# Escapes ${INPUT_STRING} and writes the quoted JSON value into ${OUTPUT_VAR}.
function(_integration_escape_json_string OUTPUT_VAR INPUT_STRING)
  string(REPLACE "\\" "\\\\" _escaped "${INPUT_STRING}")
  string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
  set(${OUTPUT_VAR} "\"${_escaped}\"" PARENT_SCOPE)
endfunction()

# Serializes ${ARGN} entries into a JSON array literal stored in ${OUTPUT_VAR}.
function(_integration_list_to_json OUTPUT_VAR)
  set(_json "")
  foreach(_item IN LISTS ARGN)
    _integration_escape_json_string(_escaped_item "${_item}")
    # Append a comma separator if this is not the first element.
    if(NOT _json STREQUAL "")
      string(APPEND _json ", ")
    endif()
    string(APPEND _json "${_escaped_item}")
  endforeach()
  set(${OUTPUT_VAR} "${_json}" PARENT_SCOPE)
endfunction()

# Collects default, implicit, extra user-specified compile flags and returns them via ${OUTPUT_VAR}.
function(_integration_collect_compile_flags OUTPUT_VAR)
  set(_compile_flags)
  if(CMAKE_CXX_STANDARD)
    list(APPEND _compile_flags "-std=c++${CMAKE_CXX_STANDARD}")
  else()
    list(APPEND _compile_flags "-std=c++17")
  endif()

  set(_implicit_dirs ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
  # Clean up the implicit include directories list (remove empty entries and duplicates).
  list(REMOVE_ITEM _implicit_dirs "")
  list(REMOVE_DUPLICATES _implicit_dirs)

  foreach(_dir IN LISTS _implicit_dirs)
    if(EXISTS "${_dir}")
      list(APPEND _compile_flags "-isystem" "${_dir}")
    endif()
  endforeach()

  foreach(_extra_dir IN LISTS EAST_CONST_TEST_EXTRA_INCLUDE_DIRS)
    if(NOT _extra_dir STREQUAL "")
      list(APPEND _compile_flags "-isystem" "${_extra_dir}")
    endif()
  endforeach()

  if(EAST_CONST_TEST_EXTRA_FLAGS)
    list(APPEND _compile_flags ${EAST_CONST_TEST_EXTRA_FLAGS})
  endif()

  set(${OUTPUT_VAR} "${_compile_flags}" PARENT_SCOPE)
endfunction()

# Fills the EAST_CONST_TEST_{COMPILE_FLAGS,TOOL_ARGS,TIDY_ARGS,CLANG_TIDY_PATH}_JSON variables used by config.json from flags/tidy path.
# Takes ${COMPILE_FLAGS:LIST} and CLANG_TIDY_PATH as arguments.
# Sets:
function(_integration_populate_config_variables COMPILE_FLAGS CLANG_TIDY_PATH)
  _integration_list_to_json(_compile_flags_json ${COMPILE_FLAGS})
  set(EAST_CONST_TEST_COMPILE_FLAGS_JSON "${_compile_flags_json}" PARENT_SCOPE)

  _integration_list_to_json(_tool_args_json ${EAST_CONST_TEST_DEFAULT_TOOL_ARGS})
  set(EAST_CONST_TEST_TOOL_ARGS_JSON "${_tool_args_json}" PARENT_SCOPE)

  _integration_list_to_json(_tidy_args_json ${EAST_CONST_TEST_DEFAULT_TIDY_ARGS})
  set(EAST_CONST_TEST_TIDY_ARGS_JSON "${_tidy_args_json}" PARENT_SCOPE)

  if(CLANG_TIDY_PATH)
    set(_clang_tidy_value "${CLANG_TIDY_PATH}")
  else()
    set(_clang_tidy_value "clang-tidy")
  endif()

  _integration_escape_json_string(_clang_tidy_json "${_clang_tidy_value}")
  set(EAST_CONST_TEST_CLANG_TIDY_PATH_JSON "${_clang_tidy_json}" PARENT_SCOPE)
endfunction()

# Generates config.json at CONFIG_PATH from SOURCE_DIR/tests/integration/config.json.in.
function(_integration_write_config_file SOURCE_DIR CONFIG_PATH)
  get_filename_component(_config_dir "${CONFIG_PATH}" DIRECTORY)
  file(MAKE_DIRECTORY "${_config_dir}")
  configure_file(
    ${SOURCE_DIR}/tests/integration/config.json.in
    ${CONFIG_PATH}
    @ONLY)
endfunction()

# Calls the Python runner with --list-cases and reports names back via OUTPUT_VAR.
function(_integration_enumerate_cases OUTPUT_VAR PYTHON_EXECUTABLE SCRIPT_PATH)
  execute_process(
    COMMAND ${PYTHON_EXECUTABLE} ${SCRIPT_PATH} --list-cases
    OUTPUT_VARIABLE _cases_raw
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _list_result)

  if(NOT _list_result EQUAL 0)
    message(FATAL_ERROR "Failed to enumerate integration cases for CTest")
  endif()

  # Normalize line endings and convert the newline-separated output into a CMake list.
  string(REGEX REPLACE "[\r\n]+" ";" _case_list "${_cases_raw}")

  set(${OUTPUT_VAR} "${_case_list}" PARENT_SCOPE)
endfunction()

# Registers per-case CTest entries plus an aggregate `east-const-integration` target.
function(_integration_register_ctest_entries CASE_LIST PYTHON_EXECUTABLE SCRIPT_PATH BUILD_DIR CONFIG_PATH CLANG_TIDY_PATH)
  # Register individual CTest targets for each discovered integration case.
  foreach(_case IN LISTS CASE_LIST)
    # Sanitize the case name for use in the CTest test name (replace non-alphanumeric chars with underscores).
    string(REGEX REPLACE "[^A-Za-z0-9_-]" "_" _case_name "${_case}")
    # Define a dedicated CTest entry that runs the harness for this single case.
    add_test(
      NAME "east-const-integration-${_case_name}"
      COMMAND ${PYTHON_EXECUTABLE}
              ${SCRIPT_PATH}
              --build-dir ${BUILD_DIR}
              --config ${CONFIG_PATH}
              --clang-tidy ${CLANG_TIDY_PATH}
              --case ${_case})
  endforeach()

  add_test(
    NAME east-const-integration
    COMMAND ${PYTHON_EXECUTABLE}
            ${SCRIPT_PATH}
            --build-dir ${BUILD_DIR}
            --config ${CONFIG_PATH}
            --clang-tidy ${CLANG_TIDY_PATH})
endfunction()

# Generate the config for integration testing.
# Takes BUILD_DIR, SOURCE_DIR, PYTHON_EXECUTABLE, optional CLANG_TIDY_PATH as arguments.
# Generates config.json, registers CTest entries.
function(configure_integration_harness)
  set(options)
  set(oneValueArgs BUILD_DIR SOURCE_DIR PYTHON_EXECUTABLE CLANG_TIDY_PATH)
  cmake_parse_arguments(HARNESS "" "${oneValueArgs}" "" ${ARGN})

  if(NOT HARNESS_BUILD_DIR OR NOT HARNESS_SOURCE_DIR OR NOT HARNESS_PYTHON_EXECUTABLE)
    message(FATAL_ERROR "configure_integration_harness requires BUILD_DIR, SOURCE_DIR, and PYTHON_EXECUTABLE")
  endif()

    set(EAST_CONST_TEST_EXTRA_INCLUDE_DIRS "" CACHE STRING
      "Semicolon-separated include directories appended (via -isystem) for integration compile flags")
    set(EAST_CONST_TEST_EXTRA_FLAGS "" CACHE STRING
      "Additional compile flags appended after default integration flags")
    set(EAST_CONST_TEST_DEFAULT_TOOL_ARGS "" CACHE STRING
      "Default arguments passed to the standalone tool for integration cases")
    set(EAST_CONST_TEST_DEFAULT_TIDY_ARGS "" CACHE STRING
      "Default arguments passed to clang-tidy for integration cases")

  # Collect compile flags from CMAKE_CXX_STANDARD, implicit includes, and EAST_CONST_TEST_* cache variables.
  _integration_collect_compile_flags(_integration_compile_flags)

  set(EAST_CONST_TEST_CONFIG_PATH "${HARNESS_BUILD_DIR}/tests/integration/config.json")
  _integration_populate_config_variables("${_integration_compile_flags}" "${HARNESS_CLANG_TIDY_PATH}")
  _integration_write_config_file("${HARNESS_SOURCE_DIR}" "${EAST_CONST_TEST_CONFIG_PATH}")

  set(INTEGRATION_SCRIPT "${HARNESS_SOURCE_DIR}/tests/integration/run_integration_tests.py")
  _integration_enumerate_cases(HARNESS_CASE_LIST "${HARNESS_PYTHON_EXECUTABLE}" "${INTEGRATION_SCRIPT}")

  _integration_register_ctest_entries(
    "${HARNESS_CASE_LIST}"
    "${HARNESS_PYTHON_EXECUTABLE}"
    "${INTEGRATION_SCRIPT}"
    "${HARNESS_BUILD_DIR}"
    "${EAST_CONST_TEST_CONFIG_PATH}"
    "${HARNESS_CLANG_TIDY_PATH}")
endfunction()
