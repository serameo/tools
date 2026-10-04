## Phase 7: Python Port

### Goal
Convert the C hex editor (hexvi.c) to a Python script (hexvi.py) preserving all functionality, modes, key bindings, rendering, and cross-platform support.

### Approach
- 1:1 translation of all editor logic from C to idiomatic Python
- Single file: hexvi.py
- No external dependencies (stdlib only)

### Platform Handling
| Concern | C (original) | Python (port) |
|---------|-------------|---------------|
| Terminal raw mode (Windows) | Win32 Console API (SetConsoleMode) | ctypes wrapping kernel32 (SetConsoleMode) |
| Terminal raw mode (Linux) | POSIX termios | termios + tty modules |
| Key input (Windows) | ReadConsoleInputA + VK codes | ctypes wrapping ReadConsoleInputA |
| Key input (Linux) | read() + select() for ESC detection | os.read() + select.select() |
| Terminal size (Windows) | GetConsoleScreenBufferInfo | ctypes wrapping GetConsoleScreenBufferInfo |
| Terminal size (Linux) | ioctl TIOCGWINSZ | os.get_terminal_size() |
| Screen output | printf with ANSI escapes | sys.stdout.write with ANSI escapes |

### Key Mapping
- C portable types (hv_byte, hv_int, hv_offset) replaced by Python native int
- File buffer: bytearray (mutable, supports slice/find)
- Dirty tracking: bytearray (same as C, 1 byte per position)
- Search: bytearray.find() replaces C memcmp loop (with wrap-around)
- Output buffering: list-based write buffer (_out/_flush) replaces many printf calls

### Preserved Features
- All 4 modes: Normal, Hex Edit, ASCII Edit, Command
- All key bindings: h/j/k/l, arrows, PgUp/PgDn, g/G, i, a, :, /, n, ?
- All commands: :q, :q!, :w, :wq, :ADDR, :help, /text, /xHH
- 3-panel display with identical ANSI color scheme
- Help overlay (2 pages: key bindings + ASCII table)
- Modified-byte tracking (red highlight)
- Cursor reverse-video highlight
- Viewport scrolling

### Status: Complete

