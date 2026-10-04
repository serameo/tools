# Phase 1: Project Scaffolding and Build System

## Objective

Set up a buildable, empty skeleton of the xconf library and its test
harness, wired together with CMake, flex, and bison, so the whole
toolchain compiles cleanly on both Linux and Windows before any real
parsing logic is written.

## Tasks

1. Create the directory structure defined in the master plan
   (xcomf_plans.md, section 7).
2. Write a minimal `include/xconf.h`: opaque `xconf_t` type, and
   declarations for `xconf_init` / `xconf_free` only.
3. Write a minimal `src/xconf.c` implementing `xconf_init` / `xconf_free`
   as stubs (allocate/free an empty struct).
4. Write a trivial `src/lexer.l` (flex) that recognizes end-of-file only,
   and a trivial `src/parser.y` (bison) with a single empty start rule,
   purely to validate the flex/bison + CMake pipeline end to end.
5. Write `CMakeLists.txt`:
   - `find_package(FLEX REQUIRED)`, `find_package(BISON REQUIRED)`.
   - `FLEX_TARGET`, `BISON_TARGET`, `ADD_FLEX_BISON_DEPENDENCY`.
   - Build a static library target named `xconf`.
   - `XCONF_BUILD_TESTS` option, default ON, enabling CTest.
6. Write one trivial test, `tests/test_basic.c`, that calls
   `xconf_init()` / `xconf_free()` and asserts the handle is non-null,
   wired into CTest via `tests/CMakeLists.txt`.
7. Write a top-level `README.md` (ASCII English) with build
   instructions for:
   - Linux: install flex and bison (apt/dnf), then
     `cmake -B build && cmake --build build && ctest --test-dir build`.
   - Windows: install winflexbison (via choco or vcpkg) or use MSYS2,
     then build with the Visual Studio generator or MinGW Makefiles.
8. Build and run the tests on Linux in this environment to confirm the
   pipeline works. A Windows build cannot be verified in this sandbox,
   so the Windows instructions will be marked "unverified on Windows"
   until confirmed on an actual Windows machine.

## Files Touched

- CMakeLists.txt
- include/xconf.h
- src/xconf.c
- src/lexer.l
- src/parser.y
- tests/CMakeLists.txt
- tests/test_basic.c
- README.md

## Exit / Test Criteria

- `cmake -B build && cmake --build build` succeeds on Linux with no
  errors and no warnings from the project's own code.
- `ctest --test-dir build` passes the trivial test.
- The user confirms a successful Windows build using the documented
  steps, OR explicitly accepts Linux-only verification for this phase
  and defers Windows verification to Phase 8.

## Changes Log

- 2026-09-26: This development sandbox has no network access, so
  flex, bison, and cmake could not be installed here (package
  downloads are blocked). As a result, the full
  `cmake -B build && cmake --build build && ctest` pipeline described
  in the exit criteria could NOT be run in this environment.
  - What WAS verified here: `src/xconf.c` and `tests/test_basic.c`
    were compiled and run directly with `gcc -std=c99 -Wall -Wextra`
    (bypassing CMake/flex/bison, since the Phase 1 stub API does not
    yet call into the lexer/parser). This passed: `test_basic: OK`.
  - What was NOT verified here: that `find_package(FLEX)` /
    `find_package(BISON)` locate the tools, that `lexer.l` and
    `parser.y` are valid flex/bison input, and that the full CMake
    build succeeds end to end on Linux or Windows.
  - Action needed from the user: run the commands in README.md
    (Linux or Windows section) on a machine with flex, bison, and
    cmake installed, and report back whether
    `cmake -B build && cmake --build build && ctest --test-dir build`
    succeeds, before Phase 2 begins. Alternatively, enabling network
    access for this sandbox would let the build be verified here.
