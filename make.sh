#!/usr/bin/env -S bash -euxo pipefail

LLVM_ROOT=/opt/homebrew/Cellar/llvm/21.1.6
BUILD_DIR=build

cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_DIR="$LLVM_ROOT/lib/cmake/llvm" \
    -DClang_DIR="$LLVM_ROOT/lib/cmake/clang" \
    -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"
    # -DCMAKE_CTEST_ARGUMENTS="--output-on-failure" \

cmake --build "$BUILD_DIR"

# ./build/east-const-enforcer \
#     -fix \
#     /Users/parsa/Repositories/playground/east-const-enforcer/file.cpp -- \
#     -std=c++17 \
#     -isystem /opt/homebrew/Cellar/llvm/21.1.6/include/c++/v1 \
#     -isystem /opt/homebrew/Cellar/llvm/21.1.6/lib/clang/19/include
# cmake --build build --target test
"$BUILD_DIR"/east-const-enforcer-test
