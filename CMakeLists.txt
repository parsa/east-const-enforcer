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

set(EAST_CONST_INTEGRATION_SCRIPT
    "${CMAKE_SOURCE_DIR}/tests/integration/run_integration_tests.py")

include(cmake/EastConstTestHarness.cmake)
add_east_const_integration_tests(
  BUILD_DIR ${CMAKE_BINARY_DIR}
  SCRIPT ${EAST_CONST_INTEGRATION_SCRIPT}
  PYTHON_EXECUTABLE ${Python3_EXECUTABLE}
  CLANG_TIDY_PATH ${CLANG_TIDY_PATH}
)
