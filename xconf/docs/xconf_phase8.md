# Phase 8: Cross-Platform Validation, Packaging, Documentation Polish

## Objective (from the original plan, section 9)

"Cross-platform validation (Windows MSVC + MinGW, Linux GCC/Clang),
packaging, and documentation polish."

## Honest Starting Point

Every build confirmation so far in this project (Phases 1-10) came
from the user's own machine via a WSL-style path
(`/mnt/c/dev/c/...`) and a Linux-style shell prompt - i.e. genuine
validation has so far only ever happened on Linux (gcc), inside WSL.
Native Windows compilers (MSVC, MinGW without WSL) have not actually
been exercised yet. This sandbox has no Windows, and no network to
install any Windows toolchain even if it did - so this phase cannot
"test on Windows" directly either. What it CAN do, and does: a careful
portability audit of code that specifically tends to break on native
Windows compilers (below), CMake/compiler-flag polish, and a CI
workflow that - once pushed somewhere that runs it - gives the first
genuine automated MSVC and MinGW builds this project has had.

## Portability Audit and Fixes

1. **`lexer.l`: the #1 real-world flex-on-Windows failure.** By
   default, flex's generated code includes `<unistd.h>`, which does
   not exist on MSVC. Added `%option nounistd` (stop emitting that
   include) and `%option never-interactive` (stop emitting
   `isatty()`/`fileno()` calls for interactive-terminal detection -
   POSIX-only, and irrelevant anyway since xconf only ever scans an
   in-memory buffer via `yy_scan_string()`, never `yyin`). Without
   this fix, an MSVC build would very likely have failed outright on
   the generated lexer file.
2. **No `strtoll`/other POSIX-leaning libc calls remain.** Phase 9's
   hex/binary parsing already uses a hand-written conversion function
   rather than `strtoll`, which sidesteps any libc/locale differences
   there. Checked the rest of the codebase (`fopen`/`fclose`/`fread`/
   `fseek`/`ftell`/`malloc`/`realloc`/`free`/`snprintf`/`atof`/
   `memcpy`/`strcmp`/`strlen`/`remove`) - all standard C99, available
   on MSVC 2015+, MinGW, and any reasonably current GCC/Clang.
3. **MSVC's "unsafe CRT function" warnings.** MSVC flags `fopen()`
   and friends as "use `fopen_s` instead" by default. Rather than
   adding Windows-only `_s()` code paths, `_CRT_SECURE_NO_WARNINGS` is
   now defined for MSVC builds (library and tests) - the functions
   used are standard, portable C; this only silences a
   Windows-specific nag, it changes no behavior.
4. **Strict C99, no compiler extensions.** `CMAKE_C_EXTENSIONS OFF`
   added, so GCC/Clang build in `-std=c99` mode rather than
   `-std=gnu99` - catches an accidental GNU-only construct at
   Linux-build time instead of it only surfacing later as an
   MSVC-only failure.
5. **Per-compiler warning flags**: `/W4` under MSVC, `-Wall -Wextra`
   otherwise, applied to both the library and every test target. Note:
   `/W4` will likely produce warnings from the flex/bison-*generated*
   files (`lexer.yy.c`/`parser.tab.c`) - that code is outside this
   project's control; these are expected noise, not a sign of a real
   problem, and `/WX` (warnings-as-errors) was deliberately NOT
   enabled for that reason.

## Packaging

- `CMakeLists.txt` now declares `project(xconf VERSION 1.0.0 ...)`
  and adds `install()` rules (`GNUInstallDirs`) for the static library
  and the public header, so `cmake --install build --prefix <dir>`
  works - previously the library could only be used from inside its
  own build tree.
- The plain `Makefile` from the earlier "building without CMake" work
  already produces a static library (`libxconf.a`) with no changes
  needed for this phase.

## CI Workflow (new): `.github/workflows/ci.yml`

A GitHub Actions matrix covering all four configurations the original
plan named:
- **Linux + GCC** and **Linux + Clang** (`ubuntu-latest`, flex/bison/
  cmake via `apt-get`).
- **Windows + MSVC** (`windows-latest`, winflexbison via Chocolatey;
  the config step auto-detects whether the installed package exposes
  `win_flex`/`win_bison` or `flex`/`bison` and points CMake at
  whichever it finds, since this sandbox could not confirm which name
  the current `winflexbison3` Chocolatey package uses).
- **Windows + MinGW** (`windows-latest`, via MSYS2's MINGW64
  environment: `mingw-w64-x86_64-{gcc,cmake,flex,bison}`).

This file is **untested** - this sandbox cannot run GitHub Actions
(no network, no Windows). If this repository is pushed to GitHub,
this workflow should run automatically on the next push; please check
the Actions tab and report back what (if anything) fails, the same way
every other phase's `ctest` result has been reported back. If this
project is not hosted on GitHub, the same four jobs can be adapted
fairly directly to GitLab CI, Azure Pipelines, or run as a manual
checklist on real machines - the commands in each step are the same
ones already documented in `README.md`.

## Documentation Polish

- `README.md` gained a "Platform support" section (compiler/tool
  minimum versions, a configuration table, and the `unistd.h`
  gotcha above) and a pointer to the new CI workflow.

## Files Touched

- src/lexer.l (the two `%option` additions)
- CMakeLists.txt (version, install rules, per-compiler flags, strict
  C99)
- tests/CMakeLists.txt (matching per-compiler flags)
- .github/workflows/ci.yml (new)
- README.md (new "Platform support" section)

## What Still Needs a Human (or real CI) to Actually Confirm

- ~~A genuine MSVC build and test run~~ - DONE, see Changes Log below:
  confirmed on a real machine (Visual Studio 2022 Build Tools,
  `winflexbison3` via Chocolatey), full build and `ctest` suite
  passing. This is the first native-Windows confirmation this project
  has had.
- A genuine MinGW build without WSL - still outstanding.
- The GitHub Actions CI workflow itself - still unexercised (not yet
  pushed/run anywhere, as far as these docs know).

## Changes Log

- 2026-10-01: implemented the audit fixes and CI workflow described
  above. Re-ran the full existing test/compile sanity sweep (every
  `tests/test_*.c` file, `xconf.c`, `xconf_value.c`, `xconf_writer.c`)
  with plain gcc after the `CMakeLists.txt`/`lexer.l` changes - still
  compiles and the tests that can run standalone still pass. Could NOT
  verify: the `%option` changes against a real flex (still
  unavailable here), the CMake changes against a real `cmake` (also
  still unavailable here - this sandbox has never had it, per every
  prior phase's notes), or anything in the new CI workflow. This is
  the point where "seriously" doing Phase 8 genuinely requires
  resources this sandbox does not have; please push the CI workflow
  (or run the manual steps) and report back.

- 2026-10-02: user confirmed, on a real Windows machine (not WSL) with
  Visual Studio 2022 Build Tools:
  - `choco install winflexbison3 -y`, then `cmake -B winbuild` (the
    first attempt, before installing, failed exactly as expected with
    `Could NOT find FLEX (missing: FLEX_EXECUTABLE)` - confirming the
    error path behaves sensibly too).
  - After install and a fresh PowerShell session, `cmake -B winbuild`
    located `win_flex`/`win_bison` from `PATH` automatically - no
    `-DFLEX_EXECUTABLE`/`-DBISON_EXECUTABLE` override was needed in
    this case.
  - `cmake --build winbuild --config Debug` and
    `ctest --test-dir winbuild -C Debug --output-on-failure` both
    passed completely - every test, including the `%option
    nounistd`/`never-interactive` lexer fix and the whole grammar, on
    genuine MSVC for the first time.
  - README.md updated to present this exact sequence as the confirmed
    path, and the Platform support table updated accordingly. MinGW
    remains unconfirmed on a real machine.
