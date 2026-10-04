# xconf

A C library for reading and writing text configuration files in a
key = value format. See `docs/xcomf_plans.md` for the full format
specification and development plan.

Status: Phases 1-10 complete and confirmed (parsing, querying,
programmatic construction, save, and key/value iteration all work).
See `docs/xcomf_plans.md` section 12 onward for current status and any
phases added since.

## Requirements

- CMake >= 3.15
- A C99 compiler (GCC, Clang, or MSVC)
- flex and bison (or winflexbison on Windows)

## Building on Linux

Install flex, bison, and cmake, then build:

```
sudo apt-get install -y flex bison cmake build-essential   # Debian/Ubuntu
# or: sudo dnf install -y flex bison cmake gcc              # Fedora

cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Building on Windows

**Confirmed working** (MSVC, via Visual Studio 2022 Build Tools):

```powershell
choco install winflexbison3 -y
# open a new PowerShell window so PATH picks up win_flex/win_bison

cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

CMake's `find_package(FLEX)`/`find_package(BISON)` locate
`win_flex`/`win_bison` from `PATH` automatically after a Chocolatey
install - no `FLEX_EXECUTABLE`/`BISON_EXECUTABLE` override was needed.
If your `winflexbison3` version or install method doesn't end up on
`PATH` under those names, pass the paths explicitly instead:

```powershell
cmake -B build -G "Visual Studio 17 2022" `
  -DFLEX_EXECUTABLE="C:\path\to\win_flex.exe" `
  -DBISON_EXECUTABLE="C:\path\to\win_bison.exe"
```

Alternative install methods for flex/bison (not yet confirmed on a
real machine for this project, but should work the same way once
`win_flex`/`win_bison` or `flex`/`bison` are on `PATH`):
- via vcpkg: `vcpkg install winflexbison3`
- or flex/bison from an MSYS2 install

**MinGW** (not yet confirmed on a real machine - only MSVC has been so
far): install CMake and a compiler (MinGW-w64, or Visual Studio's
"Desktop development with C++" workload for MSVC above), then:

```
cmake -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

## Building without CMake (vendored parser, static library only)

If you'd rather not use CMake, or want a build that no longer needs
flex/bison installed at all, generate the lexer/parser output *once*
(on a machine that does have flex/bison and CMake) and commit those
generated files as plain C source. From then on, `Makefile` builds
`libxconf.a` (a static library - no `.so`) and every test straight
with `cc`/`ar`, without invoking flex, bison, or CMake again.

### 1. Generate the files once

```
cmake -B build
cmake --build build
```

This leaves three generated files in `build/`:
`lexer.yy.c`, `parser.tab.c`, `parser.tab.h`.

### 2. Vendor them into src/

```
cp build/lexer.yy.c   src/lexer.yy.c
cp build/parser.tab.c src/parser.tab.c
cp build/parser.tab.h src/parser.tab.h
```

These three files are now ordinary, portable C - `src/lexer.l` and
`src/parser.y` are only needed again if the grammar itself changes and
you want to regenerate them.

### 3. Build with the plain Makefile

```
make          # builds libxconf.a and every tests/test_*.c
make test     # builds (if needed) and runs every test
make clean    # removes all build output
```

`CC`, `AR`, and `CFLAGS` can be overridden the usual way, e.g.
`make CC=clang CFLAGS="-std=c99 -O2 -Wall"`. If `src/lexer.yy.c`,
`src/parser.tab.c`, or `src/parser.tab.h` are missing, `make` fails
immediately with a message pointing back to this section, rather than
a confusing compiler error.

Re-run steps 1-2 (and commit the refreshed files) whenever
`src/lexer.l` or `src/parser.y` change.

## Platform support

| Platform | Compiler         | Status                                   |
|----------|------------------|-------------------------------------------|
| Linux    | GCC              | Validated (the primary development loop) |
| Linux    | Clang            | Expected to work; covered by CI below    |
| Windows  | MSVC             | **Validated** on a real machine (Visual Studio 2022 Build Tools, `winflexbison3` via Chocolatey) - full build and `ctest` suite passing |
| Windows  | MinGW            | Expected to work; not yet confirmed on a real machine - covered by CI below |

Minimum versions: CMake >= 3.15, a C99 compiler (GCC 7+, Clang 6+,
MSVC 2015+, or MinGW-w64), flex >= 2.6, bison >= 3.0 (or winflexbison
on Windows).

A known Windows-specific gotcha already handled for you: flex's
generated lexer code includes `<unistd.h>` by default, which does not
exist on MSVC. `src/lexer.l` sets `%option nounistd` (and
`%option never-interactive`, since xconf never scans from `yyin`
anyway) specifically to avoid this - if you ever regenerate the lexer
with different flex options, keep those two.

See `.github/workflows/ci.yml` for an automated build/test matrix
covering Linux (GCC, Clang) and Windows (MSVC, MinGW) - push this
repository to GitHub (or adapt the same steps to another CI system) to
get real cross-platform validation on every change, rather than
relying on manual testing alone.

## Project layout

```
xconf/
  CMakeLists.txt
  include/xconf.h
  src/
    xconf.c
    lexer.l
    parser.y
  tests/
    CMakeLists.txt
    test_basic.c
  docs/
    xcomf_plans.md
    xconf_phase1.md
```
