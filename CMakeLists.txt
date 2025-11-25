cmake_minimum_required(VERSION 3.28)
project(EastConstEnforcer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(include)
add_definitions(${LLVM_DEFINITIONS})

# Create a library for EastConstEnforcer that can be shared between executables
add_library(east-const-lib STATIC
  src/EastConstEnforcer.cpp)
set_target_properties(east-const-lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(east-const-lib PUBLIC include)

target_link_libraries(east-const-lib PRIVATE
  clangTooling
  clangASTMatchers
  clangBasic
  clangFrontend)

# Main executable
add_executable(east-const-enforcer
  src/prog.cpp)

target_link_libraries(east-const-enforcer PRIVATE
  east-const-lib
  clangTooling
  clangASTMatchers
  clangBasic
  clangFrontend)

add_library(east-const-tidy MODULE
  src/EastConstTidyModule.cpp)
target_link_libraries(east-const-tidy PRIVATE
  east-const-lib
  clangTidy
  clangTidyUtils
  clangTooling
  clangASTMatchers
  clangFrontend
  clangBasic)

# Add GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Add test executable
add_executable(east-const-enforcer-test
  tests/EastConstEnforcerTest.cpp
  tests/WesterlyRoundTripTest.cpp)
target_link_libraries(east-const-enforcer-test PRIVATE
  east-const-lib
  clangTooling
  clangBasic
  clangASTMatchers
  gtest_main
  pthread)

include(GoogleTest)
gtest_discover_tests(east-const-enforcer-test)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOT DEFINED CLANG_TIDY_PATH OR NOT CLANG_TIDY_PATH)
  if(LLVM_TOOLS_BINARY_DIR AND EXISTS "${LLVM_TOOLS_BINARY_DIR}/clang-tidy")
    set(CLANG_TIDY_PATH "${LLVM_TOOLS_BINARY_DIR}/clang-tidy")
  else()
    find_program(CLANG_TIDY_PATH_DEFAULT NAMES clang-tidy)
    if(CLANG_TIDY_PATH_DEFAULT)
      set(CLANG_TIDY_PATH "${CLANG_TIDY_PATH_DEFAULT}")
    else()
      set(CLANG_TIDY_PATH "clang-tidy")
    endif()
  endif()
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

function(east_const_list_to_json OUTPUT_VAR)
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

east_const_list_to_json(EAST_CONST_COMPILE_FLAGS_JSON ${EAST_CONST_INTEGRATION_COMPILE_FLAGS})
east_const_list_to_json(EAST_CONST_TOOL_ARGS_JSON ${EAST_CONST_INTEGRATION_DEFAULT_TOOL_ARGS})
east_const_list_to_json(EAST_CONST_TIDY_ARGS_JSON ${EAST_CONST_INTEGRATION_DEFAULT_TIDY_ARGS})

function(east_const_escape_json_string OUTPUT_VAR INPUT_STRING)
  string(REPLACE "\\" "\\\\" _escaped "${INPUT_STRING}")
  string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
  set(${OUTPUT_VAR} "\"${_escaped}\"" PARENT_SCOPE)
endfunction()

east_const_escape_json_string(EAST_CONST_CLANG_TIDY_PATH_JSON "${CLANG_TIDY_PATH}")

set(EAST_CONST_INTEGRATION_CONFIG "${CMAKE_BINARY_DIR}/tests/integration/config.json")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/integration")
configure_file(
  ${CMAKE_SOURCE_DIR}/tests/integration/config.json.in
  ${EAST_CONST_INTEGRATION_CONFIG}
  @ONLY)

set(EAST_CONST_INTEGRATION_SCRIPT
    "${CMAKE_SOURCE_DIR}/tests/integration/run_integration_tests.py")

execute_process(
  COMMAND ${Python3_EXECUTABLE} ${EAST_CONST_INTEGRATION_SCRIPT} --list-cases
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
    COMMAND ${Python3_EXECUTABLE}
            ${EAST_CONST_INTEGRATION_SCRIPT}
            --build-dir ${CMAKE_BINARY_DIR}
            --config ${EAST_CONST_INTEGRATION_CONFIG}
            --clang-tidy ${CLANG_TIDY_PATH}
            --case ${EAST_CONST_CASE})
endforeach()

add_test(
  NAME east-const-integration
  COMMAND ${Python3_EXECUTABLE}
          ${EAST_CONST_INTEGRATION_SCRIPT}
          --build-dir ${CMAKE_BINARY_DIR}
          --config ${EAST_CONST_INTEGRATION_CONFIG}
          --clang-tidy ${CLANG_TIDY_PATH}
)
