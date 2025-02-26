#!/usr/bin/env -S bash -euxo pipefail
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_DIR=/opt/homebrew/Cellar/llvm/19.1.7_1/lib/cmake/llvm \
    -DClang_DIR=/opt/homebrew/Cellar/llvm/19.1.7_1/lib/cmake/clang
cmake --build build
./build/east-const-enforcer -fix /Users/parsa/Repositories/playground/east-const-enforcer/file.cpp -- --std=c++17