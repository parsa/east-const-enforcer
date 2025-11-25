# East Const Enforcer

## Purpose
- **Const style enforcement:** Builds on Clang tooling to identify declarations that place `const` on the left ("west const") and prepares replacements that rewrite them into the preferred "east const" form (`type const`).
- **Comprehensive coverage goal:** Aims to operate across variables, function signatures, class members, typedefs, and alias declarations so the codebase adheres to consistent east-const style.

## Current State
- **Transformation logic feature-complete:** Core handlers in `src/EastConstEnforcer.cpp` now rewrite variables, function signatures (return + params), class members, typedefs, using-aliases, template arguments, and non-type template parameters while preserving existing east-const tokens.
- **Tests green:** Running `./make.sh` (or `./build/east-const-enforcer-test`) executes the full GoogleTest suite in `tests/EastConstEnforcerTest.cpp`, and all suites currently pass.
- **Edge-case coverage:** Complex pointer/reference combinations, nested template arguments, and mixed `const`/`volatile` qualifiers are handled; only macro-expanded code and compiler-generated ranges remain intentionally untouched.

## Build & Test Workflow
- **Prerequisites:** LLVM/Clang CMake packages (currently targeting the Homebrew installs at `/opt/homebrew/Cellar/llvm/21.1.6/lib/cmake/{llvm,clang}`) and a C++17-capable toolchain.
- **One-shot script:** Run `./make.sh` to configure (Release build), compile, and execute the test binary.
- **Manual CMake sequence:**
  1. `cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=/…/cmake/llvm -DClang_DIR=/…/cmake/clang`
  2. `cmake --build build`
  3. Optional: `cmake --build build --target test`
  4. Optional direct invocation example (with includes adjusted for your LLVM install):
     ```bash
     ./build/east-const-enforcer -fix path/to/file.cpp -- \
       -std=c++17 \
       -isystem /opt/homebrew/Cellar/llvm/21.1.6/include/c++/v1 \
       -isystem /opt/homebrew/Cellar/llvm/21.1.6/lib/clang/19/include
     ```
- **Clang-Tidy plugin build & usage:**
  - The `east-const-tidy` module (built automatically with the regular targets) lives at `build/libeast-const-tidy.dylib` on macOS.
  - Load it with clang-tidy: `clang-tidy -load ./build/libeast-const-tidy.dylib -checks=-*,east-const-enforcer file.cpp -- <compile flags>`.
  - Provide the same compile commands (typically via `compile_commands.json`) that you would supply to the standalone refactoring tool.

## Tests to Keep Green
- **Executable:** `./build/east-const-enforcer-test`
- **Suites:**
  - `HandlesSimpleTypes`
  - `HandlesStdStringTypes`
  - `HandlesComplexPointerTypes`
  - `HandlesFunctionSignatures`
  - `HandlesContainerTypes`
  - `HandlesClassMembers`
  - `HandlesTemplateTypes`
  - `HandlesArrayTypes`
  - `HandlesTypedefAndUsing`
  - `NegativeTestCases`
  - `HandlesEdgeCases`
