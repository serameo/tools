/*
 * Filename: hexvi.c - Vi-style Hex Editor
 * Author: Seree Rakwong [AI]
 * Date: 22-SEP-2026
 *
 * A terminal hex editor using ANSI escape codes.
 * 3-panel display: Address | Hex Bytes | ASCII
 *
 * Compile (Windows): cl /Fe:hexvi.exe hexvi.c
 * Compile (Linux):   gcc -o hexvi hexvi.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>
#endif

/* ------------------------------------------------------------------ */
/*  Portable type definitions                                          */
/* ------------------------------------------------------------------ */
typedef unsigned char   hv_byte;      /* raw byte / buffer element */
typedef int             hv_int;       /* general integer */
typedef long            hv_offset;    /* file offset / size (up to 2GB) */
typedef const char     *hv_cstr;      /* read-only string pointer */

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */
#define BYTES_PER_LINE  16
#define ADDR_WIDTH      10  /* "00000000  " */
#define HEX_WIDTH       49  /* 16*3 + 1 extra space between byte 7-8 */
#define ASCII_WIDTH     16
#define MAX_CMD_LEN     256

/* ------------------------------------------------------------------ */
/*  Editor modes                                                       */
/* ------------------------------------------------------------------ */
enum {
    MODE_NORMAL = 0,
    MODE_EDIT,
    MODE_ASCII,
    MODE_COMMAND
};

/* ------------------------------------------------------------------ */
/*  Editor state                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    /* File data */
    hv_byte    *buf;
    hv_byte    *dirty;          /* 1 per byte: non-zero = modified */
    hv_offset   file_size;
    char        filename[260];
    hv_int      modified;

    /* Display */
    hv_int      term_rows;
    hv_int      term_cols;
    hv_int      view_lines;     /* data lines visible */
    hv_offset   viewport;       /* first byte offset shown (aligned to 16) */

    /* Cursor */
    hv_offset   cursor;         /* byte offset in file */
    hv_int      nibble;         /* 0=high, 1=low nibble (edit mode) */

    /* Mode */
    hv_int      mode;
    char        status_msg[256];
    char        cmd_buf[MAX_CMD_LEN];
    hv_int      cmd_len;

    /* Search */
    hv_byte     search_buf[MAX_CMD_LEN];
    hv_int      search_len;
    hv_int      search_is_hex;

    /* Running */
    hv_int      running;
} Editor;

static Editor ed;

/* ------------------------------------------------------------------ */
/*  Platform: terminal raw mode                                        */
/* ------------------------------------------------------------------ */
#ifdef _WIN32
static HANDLE  hStdin;
static HANDLE  hStdout;
static DWORD   orig_in_mode;
static DWORD   orig_out_mode;

static void term_init(void)
{
    hStdin  = GetStdHandle(STD_INPUT_HANDLE);
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hStdin,  &orig_in_mode);
    GetConsoleMode(hStdout, &orig_out_mode);

    /* Raw input: no line input, no echo, no VT input (keep VK codes for arrows) */
    SetConsoleMode(hStdin, ENABLE_WINDOW_INPUT);
    /* Enable ANSI output only */
    SetConsoleMode(hStdout,
        orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);

    printf("\033[?1049h");
    printf("\033[?25l");
    fflush(stdout);
}

static void term_cleanup(void)
{
    printf("\033[?25h");
    printf("\033[?1049l");
    fflush(stdout);
    SetConsoleMode(hStdin,  orig_in_mode);
    SetConsoleMode(hStdout, orig_out_mode);
}

static void get_terminal_size(hv_int *rows, hv_int *cols)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hStdout, &csbi);
    *cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
}

#define KEY_UP      (256 + VK_UP)
#define KEY_DOWN    (256 + VK_DOWN)
#define KEY_LEFT    (256 + VK_LEFT)
#define KEY_RIGHT   (256 + VK_RIGHT)
#define KEY_PGUP    (256 + VK_PRIOR)
#define KEY_PGDN    (256 + VK_NEXT)
#define KEY_ESC     27
#define KEY_ENTER   13
#define KEY_BACKSPACE 8

static hv_int read_key(void)
{
    INPUT_RECORD rec;
    DWORD count;
    for (;;) {
        ReadConsoleInputA(hStdin, &rec, 1, &count);
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
            char ch = rec.Event.KeyEvent.uChar.AsciiChar;
            if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT ||
                vk == VK_PRIOR || vk == VK_NEXT)
                return 256 + vk;
            if (ch != 0)
                return (hv_byte)ch;
        }
    }
}

#else /* Linux / POSIX */
static struct termios orig_termios;
static volatile hv_int g_resized = 0;

static void sigwinch_handler(int sig)
{
    (void)sig;
    g_resized = 1;
}

static void term_init(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    signal(SIGWINCH, sigwinch_handler);
    printf("\033[?1049h");
    printf("\033[?25l");
    fflush(stdout);
}

static void term_cleanup(void)
{
    printf("\033[?25h");
    printf("\033[?1049l");
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void get_terminal_size(hv_int *rows, hv_int *cols)
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    *rows = ws.ws_row;
    *cols = ws.ws_col;
}

#define KEY_UP      1000
#define KEY_DOWN    1001
#define KEY_LEFT    1002
#define KEY_RIGHT   1003
#define KEY_PGUP    1004
#define KEY_PGDN    1005
#define KEY_ESC     27
#define KEY_ENTER   13
#define KEY_BACKSPACE 8

static hv_int read_key(void)
{
    hv_byte c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    if (c == 27) {
        /* Use select() to check if escape sequence follows (50ms timeout) */
        fd_set fds;
        struct timeval tv;
        hv_byte seq[3];
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0)
            return KEY_ESC;
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESC;
        if (seq[0] != '[') return KEY_ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESC;
        if (seq[1] == 'A') return KEY_UP;
        if (seq[1] == 'B') return KEY_DOWN;
        if (seq[1] == 'C') return KEY_RIGHT;
        if (seq[1] == 'D') return KEY_LEFT;
        if (seq[1] == '5') { read(STDIN_FILENO, &seq[2], 1); return KEY_PGUP; }
        if (seq[1] == '6') { read(STDIN_FILENO, &seq[2], 1); return KEY_PGDN; }
        return KEY_ESC;
    }
    if (c == 13) return KEY_ENTER;
    if (c == 8)  return KEY_BACKSPACE;
    return c;
}
#endif /* _WIN32 */

/* ------------------------------------------------------------------ */
/*  File I/O                                                           */
/* ------------------------------------------------------------------ */
static hv_int load_file(hv_cstr path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    ed.file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    ed.buf = (hv_byte *)malloc(ed.file_size > 0 ? ed.file_size : 1);
    if (!ed.buf) {
        fclose(f);
        fprintf(stderr, "Out of memory\n");
        return -1;
    }
    ed.dirty = (hv_byte *)calloc(ed.file_size > 0 ? ed.file_size : 1, 1);
    if (!ed.dirty) {
        free(ed.buf);
        fclose(f);
        fprintf(stderr, "Out of memory\n");
        return -1;
    }
    if (ed.file_size > 0)
        fread(ed.buf, 1, ed.file_size, f);
    fclose(f);

    strncpy(ed.filename, path, sizeof(ed.filename) - 1);
    ed.filename[sizeof(ed.filename) - 1] = '\0';
    ed.modified = 0;
    return 0;
}

static hv_int save_file(void)
{
    FILE *f = fopen(ed.filename, "wb");
    if (!f) {
        sprintf(ed.status_msg, "Error: Cannot write to %s", ed.filename);
        return -1;
    }
    fwrite(ed.buf, 1, ed.file_size, f);
    fclose(f);
    memset(ed.dirty, 0, ed.file_size > 0 ? ed.file_size : 1);
    ed.modified = 0;
    sprintf(ed.status_msg, "Written %ld bytes to %s", ed.file_size, ed.filename);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Screen rendering                                                   */
/* ------------------------------------------------------------------ */
static void move_cursor_to(hv_int row, hv_int col)
{
    printf("\033[%d;%dH", row, col);
}

static void clear_screen(void)
{
    printf("\033[2J");
}

static void draw_line(hv_int screen_row, hv_offset offset)
{
    hv_int i;
    move_cursor_to(screen_row, 1);

    /* Address column */
    printf("\033[36m%08lX\033[0m  ", offset);

    /* Hex bytes */
    for (i = 0; i < BYTES_PER_LINE; i++) {
        if (i == 8) printf(" ");
        if (offset + i < ed.file_size) {
            hv_int is_cursor = (offset + i == ed.cursor);
            hv_int is_dirty  = ed.dirty[offset + i];
            if (is_cursor && is_dirty)
                printf("\033[1;31;7m%02X\033[0m ", ed.buf[offset + i]);
            else if (is_cursor)
                printf("\033[7m%02X\033[0m ", ed.buf[offset + i]);
            else if (is_dirty)
                printf("\033[31m%02X\033[0m ", ed.buf[offset + i]);
            else
                printf("%02X ", ed.buf[offset + i]);
        } else {
            printf("   ");
        }
    }

    printf(" ");

    /* ASCII column */
    for (i = 0; i < BYTES_PER_LINE; i++) {
        if (offset + i < ed.file_size) {
            hv_byte c = ed.buf[offset + i];
            hv_int  is_cursor = (offset + i == ed.cursor);
            hv_int  is_dirty  = ed.dirty[offset + i];
            char    ch = (c >= 0x20 && c <= 0x7E) ? (char)c : '.';
            if (is_cursor && is_dirty)
                printf("\033[1;31;7m%c\033[0m", ch);
            else if (is_cursor)
                printf("\033[7m%c\033[0m", ch);
            else if (is_dirty)
                printf("\033[31m%c\033[0m", ch);
            else
                printf("\033[32m%c\033[0m", ch);
        } else {
            putchar(' ');
        }
    }

    printf("\033[K");
}

static void draw_status(void)
{
    hv_cstr mode_str;

    move_cursor_to(ed.term_rows - 1, 1);

    switch (ed.mode) {
        case MODE_EDIT:    mode_str = "-- EDIT --";    break;
        case MODE_ASCII:   mode_str = "-- ASCII --";   break;
        case MODE_COMMAND: mode_str = "-- COMMAND --"; break;
        default:           mode_str = "-- NORMAL --";  break;
    }

    printf("\033[7m");
    printf(" %-12s | %s%s | Size: %ld | Offset: 0x%08lX (%ld)",
           mode_str,
           ed.filename,
           ed.modified ? " [+]" : "",
           ed.file_size,
           ed.cursor,
           ed.cursor);
    if (ed.file_size > 0 && ed.cursor < ed.file_size) {
        hv_byte b = ed.buf[ed.cursor];
        char ch = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';
        printf(" | Byte: 0x%02X %3d '%c'", b, b, ch);
    }
    printf("\033[K\033[0m");

    move_cursor_to(ed.term_rows, 1);
    if (ed.mode == MODE_COMMAND) {
        printf("\033[33m%s\033[0m\033[K", ed.cmd_buf);
    } else if (ed.status_msg[0]) {
        printf("%s\033[K", ed.status_msg);
    } else {
        printf("\033[K");
    }
}

static void update_viewport(void)
{
    if (ed.cursor < 0) ed.cursor = 0;
    if (ed.cursor >= ed.file_size && ed.file_size > 0)
        ed.cursor = ed.file_size - 1;
    if (ed.file_size == 0) ed.cursor = 0;

    if (ed.cursor < ed.viewport)
        ed.viewport = (ed.cursor / BYTES_PER_LINE) * BYTES_PER_LINE;

    if (ed.cursor >= ed.viewport + (hv_offset)ed.view_lines * BYTES_PER_LINE)
        ed.viewport = ((ed.cursor / BYTES_PER_LINE) - ed.view_lines + 1) * BYTES_PER_LINE;

    if (ed.viewport < 0) ed.viewport = 0;
}

static void draw_screen(void)
{
    hv_int    row;
    hv_offset offset;

    get_terminal_size(&ed.term_rows, &ed.term_cols);
    ed.view_lines = ed.term_rows - 2;
    if (ed.view_lines < 1) ed.view_lines = 1;

    update_viewport();

    for (row = 0; row < ed.view_lines; row++) {
        offset = ed.viewport + (hv_offset)row * BYTES_PER_LINE;
        if (offset < ed.file_size)
            draw_line(row + 1, offset);
        else {
            move_cursor_to(row + 1, 1);
            printf("~\033[K");
        }
    }

    draw_status();

    {
        hv_int cursor_line = (hv_int)((ed.cursor - ed.viewport) / BYTES_PER_LINE);
        hv_int cursor_col  = (hv_int)((ed.cursor - ed.viewport) % BYTES_PER_LINE);
        hv_int screen_col;

        screen_col = ADDR_WIDTH + cursor_col * 3 + 1;
        if (cursor_col >= 8) screen_col += 1;

        if (ed.mode == MODE_EDIT && ed.nibble == 1)
            screen_col += 1;

        move_cursor_to(cursor_line + 1, screen_col);
        printf("\033[?25h");
    }

    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/*  Help overlay                                                       */
/* ------------------------------------------------------------------ */
static void show_help(void)
{
    hv_int r, i, key;

    /* Page 1: Key bindings */
    clear_screen();
    r = 1;
    move_cursor_to(r++, 1); printf("\033[1;36m  HexVi - Help (1/2)\033[0m");
    move_cursor_to(r++, 1); printf("\033[K");
    move_cursor_to(r++, 1); printf("  \033[1mNormal Mode:\033[0m");
    move_cursor_to(r++, 1); printf("    h / Left      Move left");
    move_cursor_to(r++, 1); printf("    l / Right     Move right");
    move_cursor_to(r++, 1); printf("    k / Up        Move up");
    move_cursor_to(r++, 1); printf("    j / Down      Move down");
    move_cursor_to(r++, 1); printf("    PgUp / PgDn   Page scroll");
    move_cursor_to(r++, 1); printf("    g             Go to first byte");
    move_cursor_to(r++, 1); printf("    G             Go to last byte");
    move_cursor_to(r++, 1); printf("    i             Enter hex edit mode");
    move_cursor_to(r++, 1); printf("    a             Enter ASCII edit mode");
    move_cursor_to(r++, 1); printf("    :             Enter command mode");
    move_cursor_to(r++, 1); printf("    /             Search");
    move_cursor_to(r++, 1); printf("    n             Find next");
    move_cursor_to(r++, 1); printf("    ?             This help");
    move_cursor_to(r++, 1); printf("\033[K");
    move_cursor_to(r++, 1); printf("  \033[1mHex Edit Mode (i):\033[0m");
    move_cursor_to(r++, 1); printf("    0-9, a-f      Write hex nibble");
    move_cursor_to(r++, 1); printf("    Arrows        Move cursor");
    move_cursor_to(r++, 1); printf("    ESC           Back to normal");
    move_cursor_to(r++, 1); printf("\033[K");
    move_cursor_to(r++, 1); printf("  \033[1mASCII Edit Mode (a):\033[0m");
    move_cursor_to(r++, 1); printf("    Any printable Type ASCII byte directly");
    move_cursor_to(r++, 1); printf("    Arrows        Move cursor");
    move_cursor_to(r++, 1); printf("    ESC           Back to normal");
    move_cursor_to(r++, 1); printf("\033[K");
    move_cursor_to(r++, 1); printf("  \033[1mCommand Mode:\033[0m");
    move_cursor_to(r++, 1); printf("    :q  :q!  :w  :wq  :ADDR  :help");
    move_cursor_to(r++, 1); printf("    /xFFAB (hex search)  /TEXT (ASCII search)");
    move_cursor_to(r++, 1); printf("\033[K");
    move_cursor_to(r++, 1); printf("  \033[33mPress any key for ASCII table, ESC to return...\033[0m");
    fflush(stdout);
    key = read_key();
    if (key == KEY_ESC) { clear_screen(); return; }

    /* Page 2: ASCII table */
    clear_screen();
    r = 1;
    move_cursor_to(r++, 1); printf("\033[1;36m  HexVi - ASCII Table (2/2)\033[0m");
    move_cursor_to(r++, 1); printf("\033[K");
    move_cursor_to(r++, 1);
    printf("  \033[1m Hex Dec Char    Hex Dec Char    Hex Dec Char    Hex Dec Char\033[0m");
    for (i = 0; i < 32; i++) {
        char c0, c1, c2, c3;
        move_cursor_to(r++, 1);

        c0 = (i >= 0x20 && i <= 0x7E) ? (char)i : '.';
        c1 = (i + 32 >= 0x20 && i + 32 <= 0x7E) ? (char)(i + 32) : '.';
        c2 = (i + 64 >= 0x20 && i + 64 <= 0x7E) ? (char)(i + 64) : '.';
        c3 = (i + 96 >= 0x20 && i + 96 <= 0x7E) ? (char)(i + 96) : '.';

        printf("  \033[36m %02X\033[0m  %3d %c    ", i,      i,      c0);
        printf("  \033[36m %02X\033[0m  %3d %c    ", i + 32,  i + 32,  c1);
        printf("  \033[36m %02X\033[0m  %3d %c    ", i + 64,  i + 64,  c2);
        if (i + 96 < 128)
            printf("  \033[36m %02X\033[0m  %3d %c  ", i + 96,  i + 96,  c3);
    }
    move_cursor_to(r++, 1); printf("\033[K");
    move_cursor_to(r++, 1); printf("  \033[33mPress any key to return...\033[0m");
    fflush(stdout);
    read_key();
    clear_screen();
}

/* ------------------------------------------------------------------ */
/*  Cursor movement                                                    */
/* ------------------------------------------------------------------ */
static void cursor_move(hv_offset delta)
{
    hv_offset next = ed.cursor + delta;
    if (next < 0) next = 0;
    if (next >= ed.file_size) next = ed.file_size > 0 ? ed.file_size - 1 : 0;
    ed.cursor = next;
}

static void cursor_page_up(void)
{
    cursor_move(-(hv_offset)ed.view_lines * BYTES_PER_LINE);
}

static void cursor_page_down(void)
{
    cursor_move((hv_offset)ed.view_lines * BYTES_PER_LINE);
}

/* ------------------------------------------------------------------ */
/*  Hex nibble helper                                                  */
/* ------------------------------------------------------------------ */
static hv_int hex_nibble(hv_int key)
{
    if (key >= '0' && key <= '9') return key - '0';
    if (key >= 'a' && key <= 'f') return key - 'a' + 10;
    if (key >= 'A' && key <= 'F') return key - 'A' + 10;
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Search                                                             */
/* ------------------------------------------------------------------ */
static hv_int parse_hex_search(const char *str, hv_byte *out, hv_int max_len)
{
    hv_int len = 0;
    while (*str && len < max_len) {
        hv_int hi, lo;
        while (*str == ' ') str++;
        if (!*str) break;
        hi = hex_nibble(*str);
        if (hi < 0) break;
        str++;
        if (!*str) break;
        lo = hex_nibble(*str);
        if (lo < 0) break;
        str++;
        out[len++] = (hv_byte)((hi << 4) | lo);
    }
    return len;
}

static void search_forward(void)
{
    hv_offset start = ed.cursor + 1;
    hv_offset i;

    if (ed.search_len == 0) {
        sprintf(ed.status_msg, "No previous search");
        return;
    }

    for (i = start; i <= ed.file_size - ed.search_len; i++) {
        if (memcmp(ed.buf + i, ed.search_buf, ed.search_len) == 0) {
            ed.cursor = i;
            sprintf(ed.status_msg, "Found at 0x%08lX", i);
            return;
        }
    }
    /* Wrap around from beginning */
    for (i = 0; i < start && i <= ed.file_size - ed.search_len; i++) {
        if (memcmp(ed.buf + i, ed.search_buf, ed.search_len) == 0) {
            ed.cursor = i;
            sprintf(ed.status_msg, "Found at 0x%08lX (wrapped)", i);
            return;
        }
    }

    sprintf(ed.status_msg, "Not found");
}

/* ------------------------------------------------------------------ */
/*  Command execution                                                  */
/* ------------------------------------------------------------------ */
static void execute_command(void)
{
    char *cmd = ed.cmd_buf;

    if (cmd[0] == ':') {
        cmd++;
        if (strcmp(cmd, "q") == 0) {
            if (ed.modified) {
                sprintf(ed.status_msg, "Unsaved changes! Use :q! to force quit");
            } else {
                ed.running = 0;
            }
        }
        else if (strcmp(cmd, "q!") == 0) {
            ed.running = 0;
        }
        else if (strcmp(cmd, "w") == 0) {
            save_file();
        }
        else if (strcmp(cmd, "wq") == 0) {
            if (save_file() == 0)
                ed.running = 0;
        }
        else if (strcmp(cmd, "help") == 0) {
            ed.cmd_buf[0] = '\0';
            ed.cmd_len = 0;
            ed.mode = MODE_NORMAL;
            show_help();
            return;
        }
        else {
            /* Try as hex address goto */
            hv_offset addr = 0;
            hv_int valid = 0;
            const char *p = cmd;
            while (*p) {
                hv_int n = hex_nibble(*p);
                if (n < 0) { valid = 0; break; }
                addr = (addr << 4) | n;
                valid = 1;
                p++;
            }
            if (valid) {
                if (addr >= 0 && addr < ed.file_size) {
                    ed.cursor = addr;
                    sprintf(ed.status_msg, "Jump to 0x%08lX", addr);
                } else {
                    sprintf(ed.status_msg, "Address 0x%08lX out of range (max 0x%08lX)", addr, ed.file_size - 1);
                }
            } else {
                sprintf(ed.status_msg, "Unknown command: :%s", cmd);
            }
        }
    }
    else if (cmd[0] == '/') {
        cmd++;
        if (cmd[0] == 'x' || cmd[0] == 'X') {
            /* Hex search */
            cmd++;
            ed.search_len = parse_hex_search(cmd, ed.search_buf, MAX_CMD_LEN);
            ed.search_is_hex = 1;
            if (ed.search_len > 0) {
                ed.cursor--; /* search_forward starts at cursor+1 */
                search_forward();
            } else {
                sprintf(ed.status_msg, "Invalid hex pattern");
            }
        } else {
            /* ASCII search */
            ed.search_len = (hv_int)strlen(cmd);
            if (ed.search_len > MAX_CMD_LEN) ed.search_len = MAX_CMD_LEN;
            memcpy(ed.search_buf, cmd, ed.search_len);
            ed.search_is_hex = 0;
            if (ed.search_len > 0) {
                ed.cursor--;
                search_forward();
            } else {
                sprintf(ed.status_msg, "Empty search pattern");
            }
        }
    }

    ed.cmd_buf[0] = '\0';
    ed.cmd_len = 0;
    ed.mode = MODE_NORMAL;
}

/* ------------------------------------------------------------------ */
/*  Input handling                                                     */
/* ------------------------------------------------------------------ */
static void handle_edit_input(hv_int key)
{
    hv_int nib;

    switch (key) {
        case KEY_ESC:
            ed.mode = MODE_NORMAL;
            ed.nibble = 0;
            return;
        case 'h': case KEY_LEFT:
            cursor_move(-1);
            ed.nibble = 0;
            return;
        case 'l': case KEY_RIGHT:
            cursor_move(1);
            ed.nibble = 0;
            return;
        case 'k': case KEY_UP:
            cursor_move(-BYTES_PER_LINE);
            ed.nibble = 0;
            return;
        case 'j': case KEY_DOWN:
            cursor_move(BYTES_PER_LINE);
            ed.nibble = 0;
            return;
        case KEY_PGUP:
            cursor_page_up();
            ed.nibble = 0;
            return;
        case KEY_PGDN:
            cursor_page_down();
            ed.nibble = 0;
            return;
    }

    nib = hex_nibble(key);
    if (nib < 0) return;

    if (ed.file_size == 0) return;

    if (ed.nibble == 0) {
        ed.buf[ed.cursor] = (hv_byte)((nib << 4) | (ed.buf[ed.cursor] & 0x0F));
        ed.dirty[ed.cursor] = 1;
        ed.modified = 1;
        ed.nibble = 1;
    } else {
        ed.buf[ed.cursor] = (hv_byte)((ed.buf[ed.cursor] & 0xF0) | nib);
        ed.dirty[ed.cursor] = 1;
        ed.modified = 1;
        ed.nibble = 0;
        cursor_move(1);
    }
}

static void handle_ascii_input(hv_int key)
{
    switch (key) {
        case KEY_ESC:
            ed.mode = MODE_NORMAL;
            return;
        case KEY_LEFT:
            cursor_move(-1);
            return;
        case KEY_RIGHT:
            cursor_move(1);
            return;
        case KEY_UP:
            cursor_move(-BYTES_PER_LINE);
            return;
        case KEY_DOWN:
            cursor_move(BYTES_PER_LINE);
            return;
        case KEY_PGUP:
            cursor_page_up();
            return;
        case KEY_PGDN:
            cursor_page_down();
            return;
    }

    if (key >= 0x20 && key <= 0x7E && ed.file_size > 0) {
        ed.buf[ed.cursor] = (hv_byte)key;
        ed.dirty[ed.cursor] = 1;
        ed.modified = 1;
        cursor_move(1);
    }
}

static void handle_command_input(hv_int key)
{
    switch (key) {
        case KEY_ESC:
            ed.cmd_buf[0] = '\0';
            ed.cmd_len = 0;
            ed.mode = MODE_NORMAL;
            break;
        case KEY_ENTER:
            execute_command();
            break;
        case 8: case 127:
            if (ed.cmd_len > 1) {
                ed.cmd_len--;
                ed.cmd_buf[ed.cmd_len] = '\0';
            } else {
                ed.cmd_buf[0] = '\0';
                ed.cmd_len = 0;
                ed.mode = MODE_NORMAL;
            }
            break;
        default:
            if (key >= 32 && key < 127 && ed.cmd_len < MAX_CMD_LEN - 1) {
                ed.cmd_buf[ed.cmd_len++] = (char)key;
                ed.cmd_buf[ed.cmd_len] = '\0';
            }
            break;
    }
}

static void handle_normal_input(hv_int key)
{
    switch (key) {
        case 'h': case KEY_LEFT:
            cursor_move(-1);
            break;
        case 'l': case KEY_RIGHT:
            cursor_move(1);
            break;
        case 'k': case KEY_UP:
            cursor_move(-BYTES_PER_LINE);
            break;
        case 'j': case KEY_DOWN:
            cursor_move(BYTES_PER_LINE);
            break;
        case KEY_PGUP:
            cursor_page_up();
            break;
        case KEY_PGDN:
            cursor_page_down();
            break;
        case 'g':
            ed.cursor = 0;
            break;
        case 'G':
            ed.cursor = ed.file_size > 0 ? ed.file_size - 1 : 0;
            break;
        case 'i':
            ed.mode = MODE_EDIT;
            ed.nibble = 0;
            break;
        case 'a':
            ed.mode = MODE_ASCII;
            break;
        case ':':
            ed.mode = MODE_COMMAND;
            ed.cmd_buf[0] = ':';
            ed.cmd_buf[1] = '\0';
            ed.cmd_len = 1;
            break;
        case '/':
            ed.mode = MODE_COMMAND;
            ed.cmd_buf[0] = '/';
            ed.cmd_buf[1] = '\0';
            ed.cmd_len = 1;
            break;
        case 'n':
            search_forward();
            break;
        case '?':
            show_help();
            break;
        default:
            break;
    }
}

static void handle_input(void)
{
    hv_int key = read_key();

    ed.status_msg[0] = '\0';

    switch (ed.mode) {
        case MODE_NORMAL:
            handle_normal_input(key);
            break;
        case MODE_EDIT:
            handle_edit_input(key);
            break;
        case MODE_ASCII:
            handle_ascii_input(key);
            break;
        case MODE_COMMAND:
            handle_command_input(key);
            break;
    }
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
hv_int main(hv_int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: hexvi <filename>\n");
        return 1;
    }

    memset(&ed, 0, sizeof(ed));

    if (load_file(argv[1]) != 0)
        return 1;

    term_init();
    ed.running = 1;
    ed.mode = MODE_NORMAL;

    clear_screen();
    draw_screen();

    while (ed.running) {
        handle_input();
        if (ed.running)
            draw_screen();
    }

    term_cleanup();
    free(ed.dirty);
    free(ed.buf);
    return 0;
}
