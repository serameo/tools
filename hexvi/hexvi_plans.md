# HexVi - Hex Editor Implementation Plan

## Overview
A vi-style hex editor CLI application in C (single source file). Displays file contents in 3-panel layout: address | hex bytes | ASCII. Uses ANSI escape codes for terminal UI. Max file size 2GB loaded into memory. Cross-platform: compiles on both Windows and Linux.

## Design Decisions
- Console UI: ANSI escape codes (VT100) for output, native input per platform
- Windows input: Win32 Console API (ReadConsoleInputA with VK key codes)
- Linux input: POSIX termios raw mode with select() timeout for ESC detection
- File I/O: Load entire file into memory buffer
- File size: 32-bit offsets (long), max ~2GB
- Search: /xFFAB for hex bytes, /HELLO for ASCII text
- Portable types: hv_byte, hv_int, hv_offset, hv_cstr

## Screen Layout
```
00000000  41 42 43 44 45 46 47 48  49 4A 4B 4C 4D 4E 4F 50  ABCDEFGHIJKLMNOP
00000010  51 52 53 54 55 56 57 58  59 5A 00 01 02 03 04 05  QRSTUVWXYZ......
^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^^^
address   hex bytes (0-7)          hex bytes (8-15)         ASCII display
```
- 16 bytes per line, space between each hex byte
- 2 spaces gap between byte 7 and byte 8
- ASCII: '.' for non-printable (< 0x20 or > 0x7E)
- Colors: address=cyan, ASCII=green, modified=red, cursor=reverse video
- Status bar: mode, filename, [+] if modified, size, offset, byte value (hex/dec/char)
- Command line: yellow text

## Modes
1. Normal mode - cursor movement (default on startup)
2. Hex edit mode - hex nibble overwrite (enter with 'i', exit with ESC)
3. ASCII edit mode - direct ASCII byte input (enter with 'a', exit with ESC)
4. Command mode - commands and search (enter with ':' or '/')

## Key Bindings

### Normal Mode
| Key | Action |
|-----|--------|
| h / Left | Move cursor left 1 byte |
| j / Down | Move cursor down 1 line (16 bytes) |
| k / Up | Move cursor up 1 line (16 bytes) |
| l / Right | Move cursor right 1 byte |
| Page Up | Scroll up by screen height |
| Page Down | Scroll down by screen height |
| g | Go to first byte (top of file) |
| G | Go to last byte (end of file) |
| i | Enter hex edit mode |
| a | Enter ASCII edit mode |
| : | Enter command mode |
| / | Enter search mode |
| n | Find next match |
| ? | Show help (2 pages: keys + ASCII table) |

### Hex Edit Mode (i)
| Key | Action |
|-----|--------|
| 0-9, a-f, A-F | Overwrite hex nibble at cursor |
| ESC | Return to normal mode |
| Arrow keys | Move cursor (resets to high nibble) |
| Page Up / Page Down | Page scroll (resets to high nibble) |

### ASCII Edit Mode (a)
| Key | Action |
|-----|--------|
| Any printable (0x20-0x7E) | Write ASCII byte at cursor, advance |
| ESC | Return to normal mode |
| Arrow keys | Move cursor |
| Page Up / Page Down | Page scroll |

### Command Mode
| Command | Action |
|---------|--------|
| :q | Quit (warn if modified) |
| :q! | Force quit without saving |
| :w | Save file |
| :wq | Save and quit |
| :ADDR | Go to hex address (e.g. :0A00) |
| :help | Show help overlay |
| /xFFAB | Search hex bytes forward |
| /HELLO | Search ASCII text forward |
| ESC | Cancel command |
| Backspace | Delete last character |

## Phases (all completed)
- Phase 1: Core framework and display (hexvi_phase01.md)
- Phase 2: Cursor movement (hexvi_phase02.md)
- Phase 3: Edit mode (hexvi_phase03.md)
- Phase 4: Command mode and search (hexvi_phase04.md)
- Phase 5: Polish and Linux port (hexvi_phase05.md)
- Phase 6: ASCII edit mode and ASCII table help (hexvi_phase06.md)
- Phase 7: Python port (hexvi_phase07.md)

## Source Files
- hexviwin.c - Single source, compiles on Windows and Linux
- hexvilinux.c - Identical copy for Linux naming convention
- hexvi.py - Python port, runs on Windows and Linux

## Compile / Run
```
Windows:  cl /Fe:hexviwin.exe hexviwin.c
Linux:    gcc -o hexvilinux hexvilinux.c
Python:   python hexvi.py <filename>
```
