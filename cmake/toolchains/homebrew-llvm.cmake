# Configure LLVM/Clang discovery for local builds.
# Users can override LLVM_ROOT externally if they install LLVM elsewhere.

if(NOT DEFINED LLVM_ROOT)
  if(DEFINED ENV{LLVM_ROOT} AND NOT "$ENV{LLVM_ROOT}" STREQUAL "")
    set(LLVM_ROOT "$ENV{LLVM_ROOT}" CACHE PATH "Root of the LLVM installation" FORCE)
  else()
    set(LLVM_ROOT "/opt/homebrew/opt/llvm" CACHE PATH "Root of the LLVM installation" FORCE)
  endif()
endif()

set(LLVM_DIR "${LLVM_ROOT}/lib/cmake/llvm" CACHE PATH "Path to LLVMConfig.cmake" FORCE)
set(Clang_DIR "${LLVM_ROOT}/lib/cmake/clang" CACHE PATH "Path to ClangConfig.cmake" FORCE)

if(APPLE)
  execute_process(
    COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE _sdk_path
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(_sdk_path)
    set(CMAKE_OSX_SYSROOT "${_sdk_path}" CACHE PATH "macOS SDK path" FORCE)
  endif()
endif()
