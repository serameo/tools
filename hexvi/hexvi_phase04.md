# Phase 4: Command Mode and Search (Completed)

## Goal
Command-line mode for quit/save/goto and search mode for hex/ASCII pattern matching.

## Tasks

### 4.1 Command mode entry/exit
- ':' in normal mode enters command mode, shows ':' on command line (yellow)
- '/' in normal mode enters search mode, shows '/' on command line (yellow)
- ESC cancels and returns to normal mode
- Enter executes the command
- Backspace deletes last character (handles both byte 8 and 127)

### 4.2 Commands
- :q - quit (warn "Unsaved changes! Use :q! to force quit" if modified)
- :q! - force quit without saving
- :w - save file (write buffer to disk, clear dirty flags, show byte count)
- :wq - save and quit
- :HEXADDR - go to hex address (e.g. :0A00 jumps to offset 0x0A00)
- :help - show help overlay

### 4.3 Save file (save_file function)
- Write ed.buf to file in binary mode ("wb")
- Clear ed.dirty array with memset and ed.modified flag
- Show "Written N bytes to filename" in status message

### 4.4 Search
- /xHEXBYTES - search hex bytes (e.g. /xFF00AB), parsed by parse_hex_search()
- /TEXT - search ASCII text (e.g. /HELLO)
- Search forward from cursor+1, wrap around to beginning
- Show "Found at 0xADDRESS" or "Found at 0xADDRESS (wrapped)" or "Not found"
- Store last search pattern in ed.search_buf/search_len

### 4.5 Find next
- 'n' in normal mode calls search_forward() with stored pattern
- Shows "No previous search" if no pattern stored
