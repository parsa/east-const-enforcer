cmake_minimum_required(VERSION 3.28)
project(EastConstEnforcer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LLVM_LINK_LLVM_DYLIB ON CACHE BOOL "Prefer LLVM shared library" FORCE)
set(LLVM_LINK_CLANG_DYLIB ON CACHE BOOL "Prefer Clang shared library" FORCE)

if(CMAKE_CXX_COMPILER MATCHES "ccache")
  message(STATUS "Detected ccache compiler wrapper: parsing CCACHE_BASEDIR")
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} --print-config
    OUTPUT_VARIABLE _ccache_config
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCH "CCACHE_BASEDIR=([^\n]+)" _ccache_basedir_match "${_ccache_config}")
  if(CMAKE_MATCH_COUNT GREATER 0)
    set(_east_const_ccache_basedir "${CMAKE_MATCH_1}")
    message(STATUS "Found ccache base dir: ${_east_const_ccache_basedir}")
  else()
    set(_east_const_ccache_basedir "${CMAKE_SOURCE_DIR}")
    message(STATUS "CCACHE_BASEDIR not configured; using source dir")
  endif()
else()
  set(_east_const_ccache_basedir "${CMAKE_SOURCE_DIR}")
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(Threads REQUIRED)

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
  east-const-lib)
if(WIN32)
  # Prefer the monolithic clang-cpp import library on Windows so clang-tidy shares
  # the ClangTidyModuleRegistry with the plugin (avoids static-link registry splits).
  if(TARGET clang-cpp)
    target_link_libraries(east-const-tidy PRIVATE clang-cpp)
  else()
    foreach(_east_const_clang_lib IN ITEMS clangTidy clangTooling clangASTMatchers clangBasic clangFrontend)
      if(TARGET ${_east_const_clang_lib})
        target_link_libraries(east-const-tidy PRIVATE ${_east_const_clang_lib})
      endif()
    endforeach()
  endif()
endif()
if(APPLE)
  target_link_options(east-const-tidy PRIVATE "-undefined" "dynamic_lookup")
endif()

# Add GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Add test executable
add_executable(east-const-enforcer-test
  tests/EastConstExampleCasesTest.cpp
  tests/EastConstGridCodeGenTest.cpp
  )
target_link_libraries(east-const-enforcer-test PRIVATE
  east-const-lib
  clangTooling
  clangBasic
  clangASTMatchers
  gtest
  Threads::Threads)

include(GoogleTest)
gtest_discover_tests(east-const-enforcer-test)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(_llvm_config_exe "")
if(LLVM_TOOLS_BINARY_DIR)
  set(_candidate "${LLVM_TOOLS_BINARY_DIR}/llvm-config")
  if(EXISTS "${_candidate}")
    set(_llvm_config_exe "${_candidate}")
  endif()
endif()
if(NOT _llvm_config_exe)
  find_program(_llvm_config_exe NAMES llvm-config HINTS "${LLVM_TOOLS_BINARY_DIR}" "${LLVM_ROOT}/bin" PATHS "${LLVM_ROOT}/bin")
endif()

set(_east_const_use_rtti ON)
if(_llvm_config_exe)
  execute_process(
    COMMAND "${_llvm_config_exe}" --has-rtti
    OUTPUT_VARIABLE _llvm_has_rtti_raw
    RESULT_VARIABLE _llvm_has_rtti_result
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_llvm_has_rtti_result EQUAL 0)
    if(_llvm_has_rtti_raw STREQUAL "NO")
      set(_east_const_use_rtti OFF)
    endif()
  endif()
endif()

if(NOT _east_const_use_rtti AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
  message(STATUS "Disabling RTTI for east-const targets to match LLVM configuration")
  foreach(_east_const_target IN ITEMS east-const-lib east-const-enforcer east-const-tidy east-const-enforcer-test)
    if(TARGET ${_east_const_target})
      target_compile_options(${_east_const_target} PRIVATE -fno-rtti)
    endif()
  endforeach()
endif()

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

include(cmake/IntegrationHarnessSetup.cmake)
configure_integration_harness(
  BUILD_DIR "${CMAKE_BINARY_DIR}"
  SOURCE_DIR "${CMAKE_SOURCE_DIR}"
  PYTHON_EXECUTABLE "${Python3_EXECUTABLE}"
  CLANG_TIDY_PATH "${CLANG_TIDY_PATH}"
)
