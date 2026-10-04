# Phase 6: ASCII Edit Mode and ASCII Table Help (Completed)

## Goal
Add a new ASCII edit mode for typing characters directly as bytes, and extend the help overlay with a full ASCII reference table.

## Tasks

### 6.1 ASCII edit mode (MODE_ASCII)
- 'a' in normal mode enters ASCII edit mode
- Status bar shows "-- ASCII --"
- Any printable character (0x20-0x7E) writes its byte value at cursor and advances
- Arrow keys and PgUp/PgDn move cursor
- ESC returns to normal mode
- Modified bytes tracked with dirty flags (same as hex edit)

### 6.2 Implementation details
- New enum value MODE_ASCII added between MODE_EDIT and MODE_COMMAND
- New handler handle_ascii_input() accepts printable keys as direct byte writes
- Cursor movement keys (arrows, PgUp/PgDn) work identically to hex edit mode
- draw_status() displays "-- ASCII --" for the new mode
- handle_input() dispatches to handle_ascii_input() for MODE_ASCII

### 6.3 Multi-page help overlay
- Page 1: Key bindings for all 4 modes (Normal, Hex Edit, ASCII Edit, Command)
  - Updated 'i' label to "Enter hex edit mode"
  - Added 'a' as "Enter ASCII edit mode"
  - Added ASCII Edit Mode section
- Page 2: Full ASCII table (0x00-0x7F)
  - 4-column layout: Hex Dec Char | Hex Dec Char | Hex Dec Char | Hex Dec Char
  - 32 rows covering all 128 ASCII values
  - Hex values shown in cyan
  - Non-printable characters shown as '.'
- Navigation: ESC on page 1 returns immediately, any other key shows page 2
- Any key on page 2 returns to hex view

### 6.4 Linux port sync
- hexvilinux.c updated as identical copy of hexviwin.c
