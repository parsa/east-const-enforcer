#!/usr/bin/env -S bash -euxo pipefail
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_DIR=/opt/homebrew/Cellar/llvm/21.1.6/lib/cmake/llvm \
    -DClang_DIR=/opt/homebrew/Cellar/llvm/21.1.6/lib/cmake/clang
    # -DCMAKE_CTEST_ARGUMENTS="--output-on-failure" \

cmake --build build

# ./build/east-const-enforcer \
#     -fix \
#     /Users/parsa/Repositories/playground/east-const-enforcer/file.cpp -- \
#     -std=c++17 \
#     -isystem /opt/homebrew/Cellar/llvm/21.1.6/include/c++/v1 \
#     -isystem /opt/homebrew/Cellar/llvm/21.1.6/lib/clang/19/include
# cmake --build build --target test
./build/east-const-enforcer-test
