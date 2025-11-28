# East Const Enforcer

## Purpose
- **Const style enforcement:** Builds on Clang tooling to identify declarations that place `const` on the left ("west const") and prepares replacements that rewrite them into the preferred "east const" form (`type const`).
- **Comprehensive coverage goal:** Aims to operate across variables, function signatures, class members, typedefs, and alias declarations so the codebase adheres to consistent east-const style.

## Current State
- **Transformation logic feature-complete:** Core handlers in `src/EastConstEnforcer.cpp` now rewrite variables, function signatures (return + params), class members, typedefs, using-aliases, template arguments, and non-type template parameters while preserving existing east-const tokens.
- **Tests green:** Running `./make.sh` drives CTest, which runs the GoogleTest suites in `tests/EastConstExampleCasesTest.cpp`, `tests/EastConstGridCodeGenTest.cpp`, `tests/WesterlyRoundTripTest.cpp`, and the integration harness under `tests/integration`.
- **Edge-case coverage:** Complex pointer/reference combinations, nested template arguments, and mixed `const`/`volatile` qualifiers are handled; only macro-expanded code and compiler-generated ranges remain intentionally untouched.

## Build & Test Workflow
- **Prerequisites:** LLVM/Clang CMake packages (currently targeting the Homebrew installs at `/opt/homebrew/Cellar/llvm/21.1.6/lib/cmake/{llvm,clang}`) and a C++17-capable toolchain.
- **One-shot script:** Run `./make.sh` to invoke the `ninja-release` CMake preset (configure + build) and then execute every CTest target (unit + integration).
- **Manual CMake sequence:**
  1. `cmake --preset ninja-release`
  2. `cmake --build --preset ninja-release`
  3. `ctest --preset ninja-release`
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
  e.g., `/opt/homebrew/Cellar/llvm/21.1.6/bin/clang-tidy -load /Users/parsa/Repositories/playground/east-const-enforcer/build/libeast-const-tidy.so '-checks=-*,east-const-enforcer' 'src/EastConstEnforcer.2.cpp' -fix -- -Iinclude -std=c++17 -I /opt/homebrew/Cellar/llvm/21.1.6/include/`

### Configuring toolchains
- The preset references `cmake/toolchains/homebrew-llvm.cmake`. Adjust `LLVM_ROOT` inside that file (or export `LLVM_ROOT` in your environment) to point at a different LLVM/Clang installation.
- On macOS the toolchain automatically asks `xcrun` for the active SDK so you no longer have to pass `-DCMAKE_OSX_SYSROOT` manually.
- During configure CMake emits `build/tests/integration/config.json` containing the exact flags the integration harness should pass to compilers (default `-std=c++17` plus every implicit `-isystem` include). Override or extend the defaults via the cache variables `EAST_CONST_TEST_EXTRA_INCLUDE_DIRS`, `EAST_CONST_TEST_EXTRA_FLAGS`, `EAST_CONST_TEST_DEFAULT_TOOL_ARGS`, and `EAST_CONST_TEST_DEFAULT_TIDY_ARGS`.
- All CTest integration targets now supply `--config build/tests/integration/config.json`, so the Python runner never probes the host toolchain—everything comes from the values recorded at configure time.

## Tests to Keep Green
- **Unit executable:** `./build/east-const-enforcer-test` (the suites above)
- **Integration script:** `tests/integration/run_integration_tests.py` verifies
  - the standalone binary rewrites fixtures end-to-end,
  - the clang-tidy plugin emits diagnostics and can apply fixes via `clang-tidy -fix`, and
  - lint-only runs leave sources untouched while still warning.
- **CTest targets:** every fixture also has its own target (`east-const-integration-standalone_basic`, etc.), so you can run a single case with `ctest -R east-const-integration-clang_tidy_fix_basic` or call the script directly via `python tests/integration/run_integration_tests.py --case clang_tidy_fix_basic --build-dir build`.
