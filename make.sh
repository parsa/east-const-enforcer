#!/usr/bin/env -S bash -euxo pipefail

cmake --preset ninja-release
cmake --build --preset ninja-release
ctest --preset ninja-release
