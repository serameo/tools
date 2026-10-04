# Phase 5: Polish and Linux Port (Completed)

## Goal
Color scheme, terminal resize handling, help display, extra navigation, enhanced status bar, Linux port, and platform bug fixes.

## Tasks

### 5.1 Terminal resize handling
- Windows: get_terminal_size() queries console on each draw cycle
- Linux: SIGWINCH handler sets g_resized flag, get_terminal_size() via ioctl

### 5.2 Color scheme
- Address column: cyan (\033[36m)
- Normal hex bytes: white (default)
- Normal ASCII: green (\033[32m)
- Modified bytes: red (\033[31m)
- Modified + cursor: bold red reverse (\033[1;31;7m)
- Cursor: reverse video (\033[7m)
- Status bar: reverse video (\033[7m)
- Command line: yellow (\033[33m)

### 5.3 Help display (show_help function)
- '?' in normal mode shows help overlay
- :help command shows help overlay
- Displays all key bindings for Normal, Edit, and Command modes
- Any key dismisses and redraws hex view

### 5.4 Additional navigation
- 'g' in normal mode: go to first byte (offset 0)
- 'G' in normal mode: go to last byte (file_size - 1)

### 5.5 Enhanced status bar
- Shows byte value at cursor: hex, decimal, and ASCII character
- Format: "Byte: 0x41 65 'A'" (or '.' for non-printable)

### 5.6 Platform bug fixes
- Windows: removed ENABLE_VIRTUAL_TERMINAL_INPUT (was converting arrow keys to ESC sequences, breaking VK code detection). Now uses ENABLE_WINDOW_INPUT for raw KEY_EVENTs
- Linux: fixed KEY_ENTER from 10 to 13 (with ICRNL disabled, Enter sends CR not LF)
- Linux: added select() with 50ms timeout for ESC sequence detection (ESC alone no longer blocks forever)
- Linux: explicit CR-to-KEY_ENTER and BS mapping in read_key()
- Both: backspace handles both byte 8 and 127 in command mode

### 5.7 Linux port
- hexvilinux.c is an identical copy of hexviwin.c
- Single source compiles on both platforms via #ifdef _WIN32 / #else sections
- Added sys/select.h and signal.h includes for Linux

## Compile
```
Windows:  cl /Fe:hexviwin.exe hexviwin.c
Linux:    gcc -o hexvilinux hexvilinux.c
```
