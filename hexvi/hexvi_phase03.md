# Phase 3: Edit Mode (Completed)

## Goal
Hex editing: enter edit mode with 'i', type hex nibbles to overwrite bytes, ESC to return to normal mode. Track modified bytes visually.

## Tasks

### 3.1 Enter/exit edit mode
- 'i' in normal mode enters edit mode
- ESC in edit mode returns to normal mode
- Nibble position resets to 0 (high) on entry and on cursor movement

### 3.2 Hex nibble input
- Accept 0-9, a-f, A-F only (via hex_nibble() helper)
- First keystroke writes high nibble, second writes low nibble
- After low nibble written, advance cursor to next byte and reset to high nibble
- Update ed.buf in place

### 3.3 Modified byte tracking
- ed.dirty[] array (hv_byte per byte, allocated with calloc)
- Mark byte as dirty when edited, set ed.modified = 1
- save_file() clears dirty array and modified flag

### 3.4 Arrow key movement in edit mode
- Arrow keys, h/j/k/l, PgUp/PgDn all work in edit mode
- Any movement resets nibble to 0 (high)

### 3.5 Visual indicators
- Normal byte: white hex, green ASCII
- Cursor byte: reverse video
- Modified byte: red foreground
- Modified + cursor: bold red reverse video
- Status bar shows [+] when any byte modified
