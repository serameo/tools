# Phase 1: Core Framework and Display (Completed)

## Goal
Build the skeleton: terminal raw mode, file loading, and 3-panel hex display with scrolling viewport.

## Tasks

### 1.1 Platform includes and portable types
- Windows: windows.h, conio.h for console handle
- Linux: unistd.h, termios.h, sys/ioctl.h, sys/select.h, signal.h
- Portable types: hv_byte, hv_int, hv_offset, hv_cstr
- Define BYTES_PER_LINE = 16, ADDR_WIDTH = 10, MAX_CMD_LEN = 256

### 1.2 Terminal raw mode
- Save original console mode on startup
- Windows: ENABLE_WINDOW_INPUT for raw VK key codes, ENABLE_VIRTUAL_TERMINAL_PROCESSING for ANSI output
- Linux: termios raw mode (no echo, no canonical, no signals, no ICRNL)
- Alternate screen buffer (\033[?1049h)
- Restore original mode on exit

### 1.3 File I/O
- load_file(path): open file in binary read, get size, malloc buffer + dirty array, read all, close
- Editor state struct holds: buf, dirty, file_size, filename, modified flag

### 1.4 Screen rendering
- get_terminal_size(): query terminal rows/cols per platform
- draw_line(): render one line with address (cyan) | hex bytes | ASCII (green)
- draw_status(): bottom line with mode, filename, size, offset, byte value
- draw_screen(): render all visible lines + status bar + cursor position

### 1.5 Viewport
- viewport: first byte offset shown (aligned to 16)
- view_lines = terminal_rows - 2 (status + command line)
- update_viewport(): auto-scroll to keep cursor visible

## Compile
```
Windows:  cl /Fe:hexviwin.exe hexviwin.c
Linux:    gcc -o hexvilinux hexvilinux.c
```
