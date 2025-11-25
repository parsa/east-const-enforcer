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

# SDK_PATH=$(xcrun --sdk macosx --show-sdk-path)
# CLANG_INC=/opt/homebrew/Cellar/llvm/21.1.6/lib/clang/21/include
# LIBCXX=/opt/homebrew/Cellar/llvm/21.1.6/include/c++/v1
# W_INPUT=tests/westerly_test/002.in.cc

# if [[ -f "$W_INPUT" ]]; then
#   ./build/east-const-enforcer -fix "$W_INPUT" -- \
#     -std=c++17 \
#     -isysroot "$SDK_PATH" \
#     -isystem "$LIBCXX" \
#     -isystem "$SDK_PATH/usr/include" \
#     -isystem "$CLANG_INC"
# else
#   echo "Skipping manual fix for $W_INPUT (file not found)"
# fi

# cmake --build build --target test
"$BUILD_DIR"/east-const-enforcer-test
