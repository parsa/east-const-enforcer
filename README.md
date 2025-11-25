# East Const Enforcer

## Purpose
- **Const style enforcement:** Builds on Clang tooling to identify declarations that place `const` on the left ("west const") and prepares replacements that rewrite them into the preferred "east const" form (`type const`).
- **Comprehensive coverage goal:** Aims to operate across variables, function signatures, class members, typedefs, and alias declarations so the codebase adheres to consistent east-const style.

## Current State
- **Transformation logic feature-complete:** Core handlers in `src/EastConstEnforcer.cpp` now rewrite variables, function signatures (return + params), class members, typedefs, using-aliases, template arguments, and non-type template parameters while preserving existing east-const tokens.
- **Tests green:** Running `./make.sh` drives CTest, which runs both the GoogleTest suite in `tests/EastConstEnforcerTest.cpp` and the new integration harness under `tests/integration`.
- **Edge-case coverage:** Complex pointer/reference combinations, nested template arguments, and mixed `const`/`volatile` qualifiers are handled; only macro-expanded code and compiler-generated ranges remain intentionally untouched.

## Build & Test Workflow
- **Prerequisites:** LLVM/Clang CMake packages (currently targeting the Homebrew installs at `/opt/homebrew/Cellar/llvm/21.1.6/lib/cmake/{llvm,clang}`) and a C++17-capable toolchain.
- **One-shot script:** Run `./make.sh` to configure (Release build), compile, and execute every CTest target (unit + integration).
- **Manual CMake sequence:**
  1. `cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=/…/cmake/llvm -DClang_DIR=/…/cmake/clang`
  2. `cmake --build build`
  3. `ctest --test-dir build --output-on-failure`
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
- **Unit executable:** `./build/east-const-enforcer-test` (the suites above)
- **Integration script:** `tests/integration/run_integration_tests.py` verifies
  - the standalone binary rewrites fixtures end-to-end,
  - the clang-tidy plugin emits diagnostics and can apply fixes via `clang-tidy -fix`, and
  - lint-only runs leave sources untouched while still warning.
- **CTest targets:** every fixture also has its own target (`east-const-integration-standalone_basic`, etc.), so you can run a single case with `ctest -R east-const-integration-clang_tidy_fix_basic` or call the script directly via `python tests/integration/run_integration_tests.py --case clang_tidy_fix_basic --build-dir build`.
