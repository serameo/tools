# Phase 2: Cursor Movement - Normal Mode (Completed)

## Goal
Full cursor navigation in normal mode with auto-scroll and boundary checks.

## Tasks

### 2.1 Cursor movement keys
- h / Left arrow: move cursor left 1 byte
- l / Right arrow: move cursor right 1 byte
- j / Down arrow: move cursor down 1 line (16 bytes)
- k / Up arrow: move cursor up 1 line (16 bytes)
- g: go to first byte (offset 0)
- G: go to last byte (file_size - 1)

### 2.2 Page scrolling
- Page Up: scroll up by view_lines
- Page Down: scroll down by view_lines

### 2.3 Boundary checks
- Cannot move before byte 0
- Cannot move past file_size - 1
- cursor_move() clamps to valid range

### 2.4 Cursor highlight
- Current byte shown in reverse video in hex panel
- Corresponding ASCII character also in reverse video

### 2.5 Input handling
- Windows: ReadConsoleInputA reads KEY_EVENTs with VK codes for arrows/PgUp/PgDn
- Linux: POSIX read() with ESC sequence parsing via select() 50ms timeout
