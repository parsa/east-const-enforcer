cmake_minimum_required(VERSION 3.13)
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

add_executable(east-const-enforcer
  src/EastConstEnforcer.cpp)

target_link_libraries(east-const-enforcer
  PRIVATE
  clangTooling
  clangASTMatchers
  clangBasic
  clangFrontend)
