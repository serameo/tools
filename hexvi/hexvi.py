"""
Filename: hexvi.py - Vi-style Hex Editor
Author: Seree Rakwong [AI]
Date: 22-SEP-2026

A terminal hex editor using ANSI escape codes.
3-panel display: Address | Hex Bytes | ASCII

Usage: python hexvi.py <filename>
"""

import sys
import os
import struct

# --------------------------------------------------------------------------
#  Constants
# --------------------------------------------------------------------------
BYTES_PER_LINE = 16
ADDR_WIDTH = 10        # "00000000  "
MAX_CMD_LEN = 256

# --------------------------------------------------------------------------
#  Key codes
# --------------------------------------------------------------------------
KEY_UP = 1000
KEY_DOWN = 1001
KEY_LEFT = 1002
KEY_RIGHT = 1003
KEY_PGUP = 1004
KEY_PGDN = 1005
KEY_ESC = 27
KEY_ENTER = 13
KEY_BACKSPACE = 8

# --------------------------------------------------------------------------
#  Editor modes
# --------------------------------------------------------------------------
MODE_NORMAL = 0
MODE_EDIT = 1
MODE_ASCII = 2
MODE_COMMAND = 3

# --------------------------------------------------------------------------
#  Platform: terminal raw mode
# --------------------------------------------------------------------------
IS_WINDOWS = sys.platform == "win32"

if IS_WINDOWS:
    import msvcrt
    import ctypes
    from ctypes import wintypes

    _kernel32 = ctypes.windll.kernel32

    STD_INPUT_HANDLE = -10
    STD_OUTPUT_HANDLE = -11
    ENABLE_WINDOW_INPUT = 0x0008
    ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
    DISABLE_NEWLINE_AUTO_RETURN = 0x0008

    KEY_EVENT = 0x0001
    VK_UP = 0x26
    VK_DOWN = 0x28
    VK_LEFT = 0x25
    VK_RIGHT = 0x27
    VK_PRIOR = 0x21
    VK_NEXT = 0x22

    class KEY_EVENT_RECORD(ctypes.Structure):
        _fields_ = [
            ("bKeyDown", wintypes.BOOL),
            ("wRepeatCount", wintypes.WORD),
            ("wVirtualKeyCode", wintypes.WORD),
            ("wVirtualScanCode", wintypes.WORD),
            ("uChar", ctypes.c_char),
            ("dwControlKeyState", wintypes.DWORD),
        ]

    class INPUT_RECORD_UNION(ctypes.Union):
        _fields_ = [("KeyEvent", KEY_EVENT_RECORD)]

    class INPUT_RECORD(ctypes.Structure):
        _fields_ = [
            ("EventType", wintypes.WORD),
            ("_padding", wintypes.WORD),
            ("Event", INPUT_RECORD_UNION),
        ]

    class COORD(ctypes.Structure):
        _fields_ = [("X", ctypes.c_short), ("Y", ctypes.c_short)]

    class SMALL_RECT(ctypes.Structure):
        _fields_ = [
            ("Left", ctypes.c_short),
            ("Top", ctypes.c_short),
            ("Right", ctypes.c_short),
            ("Bottom", ctypes.c_short),
        ]

    class CONSOLE_SCREEN_BUFFER_INFO(ctypes.Structure):
        _fields_ = [
            ("dwSize", COORD),
            ("dwCursorPosition", COORD),
            ("wAttributes", wintypes.WORD),
            ("srWindow", SMALL_RECT),
            ("dwMaximumWindowSize", COORD),
        ]

    _h_stdin = _kernel32.GetStdHandle(STD_INPUT_HANDLE)
    _h_stdout = _kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
    _orig_in_mode = wintypes.DWORD()
    _orig_out_mode = wintypes.DWORD()

    def term_init():
        _kernel32.GetConsoleMode(_h_stdin, ctypes.byref(_orig_in_mode))
        _kernel32.GetConsoleMode(_h_stdout, ctypes.byref(_orig_out_mode))
        _kernel32.SetConsoleMode(_h_stdin, ENABLE_WINDOW_INPUT)
        _kernel32.SetConsoleMode(
            _h_stdout,
            _orig_out_mode.value
            | ENABLE_VIRTUAL_TERMINAL_PROCESSING
            | DISABLE_NEWLINE_AUTO_RETURN,
        )
        sys.stdout.write("\033[?1049h\033[?25l")
        sys.stdout.flush()

    def term_cleanup():
        sys.stdout.write("\033[?25h\033[?1049l")
        sys.stdout.flush()
        _kernel32.SetConsoleMode(_h_stdin, _orig_in_mode)
        _kernel32.SetConsoleMode(_h_stdout, _orig_out_mode)

    def get_terminal_size():
        csbi = CONSOLE_SCREEN_BUFFER_INFO()
        _kernel32.GetConsoleScreenBufferInfo(_h_stdout, ctypes.byref(csbi))
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1
        return rows, cols

    def read_key():
        rec = INPUT_RECORD()
        count = wintypes.DWORD()
        _VK_MAP = {
            VK_UP: KEY_UP,
            VK_DOWN: KEY_DOWN,
            VK_LEFT: KEY_LEFT,
            VK_RIGHT: KEY_RIGHT,
            VK_PRIOR: KEY_PGUP,
            VK_NEXT: KEY_PGDN,
        }
        while True:
            _kernel32.ReadConsoleInputA(
                _h_stdin, ctypes.byref(rec), 1, ctypes.byref(count)
            )
            if rec.EventType == KEY_EVENT and rec.Event.KeyEvent.bKeyDown:
                vk = rec.Event.KeyEvent.wVirtualKeyCode
                ch = rec.Event.KeyEvent.uChar
                if vk in _VK_MAP:
                    return _VK_MAP[vk]
                if ch and ch != b"\x00":
                    return ch[0]

else:
    import termios
    import tty
    import select
    import signal

    _orig_termios = None

    def _sigwinch_handler(signum, frame):
        pass

    def term_init():
        global _orig_termios
        _orig_termios = termios.tcgetattr(sys.stdin.fileno())
        tty.setraw(sys.stdin.fileno())
        signal.signal(signal.SIGWINCH, _sigwinch_handler)
        sys.stdout.write("\033[?1049h\033[?25l")
        sys.stdout.flush()

    def term_cleanup():
        sys.stdout.write("\033[?25h\033[?1049l")
        sys.stdout.flush()
        if _orig_termios is not None:
            termios.tcsetattr(
                sys.stdin.fileno(), termios.TCSAFLUSH, _orig_termios
            )

    def get_terminal_size():
        try:
            sz = os.get_terminal_size()
            return sz.lines, sz.columns
        except OSError:
            return 24, 80

    def read_key():
        fd = sys.stdin.fileno()
        c = os.read(fd, 1)
        if not c:
            return -1
        b = c[0]
        if b == 27:
            r, _, _ = select.select([fd], [], [], 0.05)
            if not r:
                return KEY_ESC
            seq0 = os.read(fd, 1)
            if not seq0 or seq0[0] != ord("["):
                return KEY_ESC
            seq1 = os.read(fd, 1)
            if not seq1:
                return KEY_ESC
            ch = seq1[0]
            if ch == ord("A"):
                return KEY_UP
            if ch == ord("B"):
                return KEY_DOWN
            if ch == ord("C"):
                return KEY_RIGHT
            if ch == ord("D"):
                return KEY_LEFT
            if ch == ord("5"):
                os.read(fd, 1)
                return KEY_PGUP
            if ch == ord("6"):
                os.read(fd, 1)
                return KEY_PGDN
            return KEY_ESC
        if b == 13:
            return KEY_ENTER
        if b == 8:
            return KEY_BACKSPACE
        if b == 127:
            return KEY_BACKSPACE
        return b


# --------------------------------------------------------------------------
#  Editor state
# --------------------------------------------------------------------------
class Editor:
    def __init__(self):
        self.buf = bytearray()
        self.dirty = bytearray()
        self.file_size = 0
        self.filename = ""
        self.modified = False

        self.term_rows = 24
        self.term_cols = 80
        self.view_lines = 22
        self.viewport = 0

        self.cursor = 0
        self.nibble = 0

        self.mode = MODE_NORMAL
        self.status_msg = ""
        self.cmd_buf = ""
        self.cmd_len = 0

        self.search_buf = bytearray()
        self.search_len = 0
        self.search_is_hex = False

        self.running = True


ed = Editor()


# --------------------------------------------------------------------------
#  File I/O
# --------------------------------------------------------------------------
def load_file(path):
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as exc:
        sys.stderr.write(f"Cannot open file: {path} ({exc})\n")
        return False

    ed.buf = bytearray(data)
    ed.file_size = len(data)
    ed.dirty = bytearray(max(ed.file_size, 1))
    ed.filename = path
    ed.modified = False
    return True


def save_file():
    try:
        with open(ed.filename, "wb") as f:
            f.write(ed.buf)
    except OSError:
        ed.status_msg = f"Error: Cannot write to {ed.filename}"
        return False

    ed.dirty = bytearray(max(ed.file_size, 1))
    ed.modified = False
    ed.status_msg = f"Written {ed.file_size} bytes to {ed.filename}"
    return True


# --------------------------------------------------------------------------
#  Output helpers
# --------------------------------------------------------------------------
_out_parts = []


def _out(s):
    _out_parts.append(s)


def _flush():
    sys.stdout.write("".join(_out_parts))
    _out_parts.clear()
    sys.stdout.flush()


# --------------------------------------------------------------------------
#  Screen rendering
# --------------------------------------------------------------------------
def move_cursor_to(row, col):
    _out(f"\033[{row};{col}H")


def clear_screen():
    _out("\033[2J")


def draw_line(screen_row, offset):
    move_cursor_to(screen_row, 1)

    _out(f"\033[36m{offset:08X}\033[0m  ")

    for i in range(BYTES_PER_LINE):
        if i == 8:
            _out(" ")
        if offset + i < ed.file_size:
            b = ed.buf[offset + i]
            is_cursor = (offset + i == ed.cursor)
            is_dirty = ed.dirty[offset + i]
            if is_cursor and is_dirty:
                _out(f"\033[1;31;7m{b:02X}\033[0m ")
            elif is_cursor:
                _out(f"\033[7m{b:02X}\033[0m ")
            elif is_dirty:
                _out(f"\033[31m{b:02X}\033[0m ")
            else:
                _out(f"{b:02X} ")
        else:
            _out("   ")

    _out(" ")

    for i in range(BYTES_PER_LINE):
        if offset + i < ed.file_size:
            c = ed.buf[offset + i]
            is_cursor = (offset + i == ed.cursor)
            is_dirty = ed.dirty[offset + i]
            ch = chr(c) if 0x20 <= c <= 0x7E else "."
            if is_cursor and is_dirty:
                _out(f"\033[1;31;7m{ch}\033[0m")
            elif is_cursor:
                _out(f"\033[7m{ch}\033[0m")
            elif is_dirty:
                _out(f"\033[31m{ch}\033[0m")
            else:
                _out(f"\033[32m{ch}\033[0m")
        else:
            _out(" ")

    _out("\033[K")


def draw_status():
    move_cursor_to(ed.term_rows - 1, 1)

    mode_strs = {
        MODE_EDIT: "-- EDIT --",
        MODE_ASCII: "-- ASCII --",
        MODE_COMMAND: "-- COMMAND --",
    }
    mode_str = mode_strs.get(ed.mode, "-- NORMAL --")

    mod_flag = " [+]" if ed.modified else ""
    _out("\033[7m")
    _out(
        f" {mode_str:<12s} | {ed.filename}{mod_flag}"
        f" | Size: {ed.file_size}"
        f" | Offset: 0x{ed.cursor:08X} ({ed.cursor})"
    )
    if ed.file_size > 0 and ed.cursor < ed.file_size:
        b = ed.buf[ed.cursor]
        ch = chr(b) if 0x20 <= b <= 0x7E else "."
        _out(f" | Byte: 0x{b:02X} {b:3d} '{ch}'")
    _out("\033[K\033[0m")

    move_cursor_to(ed.term_rows, 1)
    if ed.mode == MODE_COMMAND:
        _out(f"\033[33m{ed.cmd_buf}\033[0m\033[K")
    elif ed.status_msg:
        _out(f"{ed.status_msg}\033[K")
    else:
        _out("\033[K")


def update_viewport():
    if ed.cursor < 0:
        ed.cursor = 0
    if ed.cursor >= ed.file_size and ed.file_size > 0:
        ed.cursor = ed.file_size - 1
    if ed.file_size == 0:
        ed.cursor = 0

    if ed.cursor < ed.viewport:
        ed.viewport = (ed.cursor // BYTES_PER_LINE) * BYTES_PER_LINE

    if ed.cursor >= ed.viewport + ed.view_lines * BYTES_PER_LINE:
        ed.viewport = (
            (ed.cursor // BYTES_PER_LINE) - ed.view_lines + 1
        ) * BYTES_PER_LINE

    if ed.viewport < 0:
        ed.viewport = 0


def draw_screen():
    ed.term_rows, ed.term_cols = get_terminal_size()
    ed.view_lines = ed.term_rows - 2
    if ed.view_lines < 1:
        ed.view_lines = 1

    update_viewport()

    for row in range(ed.view_lines):
        offset = ed.viewport + row * BYTES_PER_LINE
        if offset < ed.file_size:
            draw_line(row + 1, offset)
        else:
            move_cursor_to(row + 1, 1)
            _out("~\033[K")

    draw_status()

    cursor_line = (ed.cursor - ed.viewport) // BYTES_PER_LINE
    cursor_col = (ed.cursor - ed.viewport) % BYTES_PER_LINE
    screen_col = ADDR_WIDTH + cursor_col * 3 + 1
    if cursor_col >= 8:
        screen_col += 1
    if ed.mode == MODE_EDIT and ed.nibble == 1:
        screen_col += 1

    move_cursor_to(cursor_line + 1, screen_col)
    _out("\033[?25h")

    _flush()


# --------------------------------------------------------------------------
#  Help overlay
# --------------------------------------------------------------------------
def show_help():
    clear_screen()
    r = 1

    def line(text=""):
        nonlocal r
        move_cursor_to(r, 1)
        _out(text)
        r += 1

    line("\033[1;36m  HexVi - Help (1/2)\033[0m")
    line()
    line("  \033[1mNormal Mode:\033[0m")
    line("    h / Left      Move left")
    line("    l / Right     Move right")
    line("    k / Up        Move up")
    line("    j / Down      Move down")
    line("    PgUp / PgDn   Page scroll")
    line("    g             Go to first byte")
    line("    G             Go to last byte")
    line("    i             Enter hex edit mode")
    line("    a             Enter ASCII edit mode")
    line("    :             Enter command mode")
    line("    /             Search")
    line("    n             Find next")
    line("    ?             This help")
    line()
    line("  \033[1mHex Edit Mode (i):\033[0m")
    line("    0-9, a-f      Write hex nibble")
    line("    Arrows        Move cursor")
    line("    ESC           Back to normal")
    line()
    line("  \033[1mASCII Edit Mode (a):\033[0m")
    line("    Any printable Type ASCII byte directly")
    line("    Arrows        Move cursor")
    line("    ESC           Back to normal")
    line()
    line("  \033[1mCommand Mode:\033[0m")
    line("    :q  :q!  :w  :wq  :ADDR  :help")
    line("    /xFFAB (hex search)  /TEXT (ASCII search)")
    line()
    line("  \033[33mPress any key for ASCII table, ESC to return...\033[0m")
    _flush()

    key = read_key()
    if key == KEY_ESC:
        clear_screen()
        _flush()
        return

    clear_screen()
    r = 1
    line("\033[1;36m  HexVi - ASCII Table (2/2)\033[0m")
    line()
    move_cursor_to(r, 1)
    _out(
        "  \033[1m Hex Dec Char    Hex Dec Char"
        "    Hex Dec Char    Hex Dec Char\033[0m"
    )
    r += 1

    for i in range(32):
        move_cursor_to(r, 1)
        for col_offset in (0, 32, 64, 96):
            v = i + col_offset
            if v >= 128:
                break
            ch = chr(v) if 0x20 <= v <= 0x7E else "."
            _out(f"  \033[36m {v:02X}\033[0m  {v:3d} {ch}    ")
        r += 1

    move_cursor_to(r, 1)
    r += 1
    move_cursor_to(r, 1)
    _out("  \033[33mPress any key to return...\033[0m")
    _flush()
    read_key()
    clear_screen()
    _flush()


# --------------------------------------------------------------------------
#  Cursor movement
# --------------------------------------------------------------------------
def cursor_move(delta):
    nxt = ed.cursor + delta
    if nxt < 0:
        nxt = 0
    if nxt >= ed.file_size:
        nxt = ed.file_size - 1 if ed.file_size > 0 else 0
    ed.cursor = nxt


def cursor_page_up():
    cursor_move(-ed.view_lines * BYTES_PER_LINE)


def cursor_page_down():
    cursor_move(ed.view_lines * BYTES_PER_LINE)


# --------------------------------------------------------------------------
#  Hex nibble helper
# --------------------------------------------------------------------------
def hex_nibble(key):
    if isinstance(key, int):
        ch = chr(key) if 0 <= key <= 127 else ""
    else:
        ch = key
    if "0" <= ch <= "9":
        return ord(ch) - ord("0")
    if "a" <= ch <= "f":
        return ord(ch) - ord("a") + 10
    if "A" <= ch <= "F":
        return ord(ch) - ord("A") + 10
    return -1


# --------------------------------------------------------------------------
#  Search
# --------------------------------------------------------------------------
def parse_hex_search(s):
    out = bytearray()
    i = 0
    while i < len(s):
        while i < len(s) and s[i] == " ":
            i += 1
        if i >= len(s):
            break
        hi = hex_nibble(s[i])
        if hi < 0:
            break
        i += 1
        if i >= len(s):
            break
        lo = hex_nibble(s[i])
        if lo < 0:
            break
        i += 1
        out.append((hi << 4) | lo)
    return out


def search_forward():
    if ed.search_len == 0:
        ed.status_msg = "No previous search"
        return

    start = ed.cursor + 1
    needle = bytes(ed.search_buf[: ed.search_len])

    pos = ed.buf.find(needle, start)
    if pos != -1:
        ed.cursor = pos
        ed.status_msg = f"Found at 0x{pos:08X}"
        return

    pos = ed.buf.find(needle, 0, start)
    if pos != -1:
        ed.cursor = pos
        ed.status_msg = f"Found at 0x{pos:08X} (wrapped)"
        return

    ed.status_msg = "Not found"


# --------------------------------------------------------------------------
#  Command execution
# --------------------------------------------------------------------------
def execute_command():
    cmd = ed.cmd_buf

    if cmd.startswith(":"):
        arg = cmd[1:]
        if arg == "q":
            if ed.modified:
                ed.status_msg = "Unsaved changes! Use :q! to force quit"
            else:
                ed.running = False
        elif arg == "q!":
            ed.running = False
        elif arg == "w":
            save_file()
        elif arg == "wq":
            if save_file():
                ed.running = False
        elif arg == "help":
            ed.cmd_buf = ""
            ed.cmd_len = 0
            ed.mode = MODE_NORMAL
            show_help()
            return
        else:
            addr = 0
            valid = False
            for ch in arg:
                n = hex_nibble(ch)
                if n < 0:
                    valid = False
                    break
                addr = (addr << 4) | n
                valid = True
            if valid:
                if 0 <= addr < ed.file_size:
                    ed.cursor = addr
                    ed.status_msg = f"Jump to 0x{addr:08X}"
                else:
                    ed.status_msg = (
                        f"Address 0x{addr:08X} out of range"
                        f" (max 0x{ed.file_size - 1:08X})"
                    )
            else:
                ed.status_msg = f"Unknown command: :{arg}"

    elif cmd.startswith("/"):
        arg = cmd[1:]
        if arg and arg[0] in ("x", "X"):
            pattern = parse_hex_search(arg[1:])
            ed.search_buf = pattern
            ed.search_len = len(pattern)
            ed.search_is_hex = True
            if ed.search_len > 0:
                ed.cursor -= 1
                search_forward()
            else:
                ed.status_msg = "Invalid hex pattern"
        else:
            ed.search_buf = bytearray(arg.encode("latin-1", errors="replace"))
            ed.search_len = len(ed.search_buf)
            ed.search_is_hex = False
            if ed.search_len > 0:
                ed.cursor -= 1
                search_forward()
            else:
                ed.status_msg = "Empty search pattern"

    ed.cmd_buf = ""
    ed.cmd_len = 0
    ed.mode = MODE_NORMAL


# --------------------------------------------------------------------------
#  Input handling
# --------------------------------------------------------------------------
def handle_edit_input(key):
    if key == KEY_ESC:
        ed.mode = MODE_NORMAL
        ed.nibble = 0
        return
    if key in (ord("h"), KEY_LEFT):
        cursor_move(-1)
        ed.nibble = 0
        return
    if key in (ord("l"), KEY_RIGHT):
        cursor_move(1)
        ed.nibble = 0
        return
    if key in (ord("k"), KEY_UP):
        cursor_move(-BYTES_PER_LINE)
        ed.nibble = 0
        return
    if key in (ord("j"), KEY_DOWN):
        cursor_move(BYTES_PER_LINE)
        ed.nibble = 0
        return
    if key == KEY_PGUP:
        cursor_page_up()
        ed.nibble = 0
        return
    if key == KEY_PGDN:
        cursor_page_down()
        ed.nibble = 0
        return

    nib = hex_nibble(key)
    if nib < 0:
        return
    if ed.file_size == 0:
        return

    if ed.nibble == 0:
        ed.buf[ed.cursor] = (nib << 4) | (ed.buf[ed.cursor] & 0x0F)
        ed.dirty[ed.cursor] = 1
        ed.modified = True
        ed.nibble = 1
    else:
        ed.buf[ed.cursor] = (ed.buf[ed.cursor] & 0xF0) | nib
        ed.dirty[ed.cursor] = 1
        ed.modified = True
        ed.nibble = 0
        cursor_move(1)


def handle_ascii_input(key):
    if key == KEY_ESC:
        ed.mode = MODE_NORMAL
        return
    if key == KEY_LEFT:
        cursor_move(-1)
        return
    if key == KEY_RIGHT:
        cursor_move(1)
        return
    if key == KEY_UP:
        cursor_move(-BYTES_PER_LINE)
        return
    if key == KEY_DOWN:
        cursor_move(BYTES_PER_LINE)
        return
    if key == KEY_PGUP:
        cursor_page_up()
        return
    if key == KEY_PGDN:
        cursor_page_down()
        return

    if 0x20 <= key <= 0x7E and ed.file_size > 0:
        ed.buf[ed.cursor] = key
        ed.dirty[ed.cursor] = 1
        ed.modified = True
        cursor_move(1)


def handle_command_input(key):
    if key == KEY_ESC:
        ed.cmd_buf = ""
        ed.cmd_len = 0
        ed.mode = MODE_NORMAL
    elif key == KEY_ENTER:
        execute_command()
    elif key in (8, 127):
        if ed.cmd_len > 1:
            ed.cmd_len -= 1
            ed.cmd_buf = ed.cmd_buf[: ed.cmd_len]
        else:
            ed.cmd_buf = ""
            ed.cmd_len = 0
            ed.mode = MODE_NORMAL
    else:
        if 32 <= key < 127 and ed.cmd_len < MAX_CMD_LEN - 1:
            ed.cmd_buf += chr(key)
            ed.cmd_len += 1


def handle_normal_input(key):
    if key in (ord("h"), KEY_LEFT):
        cursor_move(-1)
    elif key in (ord("l"), KEY_RIGHT):
        cursor_move(1)
    elif key in (ord("k"), KEY_UP):
        cursor_move(-BYTES_PER_LINE)
    elif key in (ord("j"), KEY_DOWN):
        cursor_move(BYTES_PER_LINE)
    elif key == KEY_PGUP:
        cursor_page_up()
    elif key == KEY_PGDN:
        cursor_page_down()
    elif key == ord("g"):
        ed.cursor = 0
    elif key == ord("G"):
        ed.cursor = ed.file_size - 1 if ed.file_size > 0 else 0
    elif key == ord("i"):
        ed.mode = MODE_EDIT
        ed.nibble = 0
    elif key == ord("a"):
        ed.mode = MODE_ASCII
    elif key == ord(":"):
        ed.mode = MODE_COMMAND
        ed.cmd_buf = ":"
        ed.cmd_len = 1
    elif key == ord("/"):
        ed.mode = MODE_COMMAND
        ed.cmd_buf = "/"
        ed.cmd_len = 1
    elif key == ord("n"):
        search_forward()
    elif key == ord("?"):
        show_help()


def handle_input():
    key = read_key()
    ed.status_msg = ""

    if ed.mode == MODE_NORMAL:
        handle_normal_input(key)
    elif ed.mode == MODE_EDIT:
        handle_edit_input(key)
    elif ed.mode == MODE_ASCII:
        handle_ascii_input(key)
    elif ed.mode == MODE_COMMAND:
        handle_command_input(key)


# --------------------------------------------------------------------------
#  Main
# --------------------------------------------------------------------------
def main():
    if len(sys.argv) < 2:
        sys.stderr.write("Usage: hexvi <filename>\n")
        return 1

    if not load_file(sys.argv[1]):
        return 1

    term_init()
    try:
        ed.running = True
        ed.mode = MODE_NORMAL

        clear_screen()
        _flush()
        draw_screen()

        while ed.running:
            handle_input()
            if ed.running:
                draw_screen()
    finally:
        term_cleanup()

    return 0


if __name__ == "__main__":
    sys.exit(main())
