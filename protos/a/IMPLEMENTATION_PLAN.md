# Implementation Plan

## Resolved Issues

### Keyboard Shortcuts (2026-01-14)

**Issue:** Missing keyboard shortcuts for common operations (Ctrl+X, Ctrl+B, Ctrl+I, Ctrl+K, Ctrl+`)

**Solution:**
- Added `cutSelection()` method that copies to clipboard and deletes selection
- Wired up existing `applyBold()`, `applyItalic()`, `applyLink()` methods to keyboard shortcuts
- Added `applyInlineCode()` method for inline code formatting
- All shortcuts properly handle both with-selection and no-selection cases

**New Shortcuts:**
| Shortcut | Action |
|----------|--------|
| Ctrl+X | Cut selection |
| Ctrl+B | Toggle bold (**text**) |
| Ctrl+I | Toggle italic (*text*) |
| Ctrl+K | Insert link [text](url) |
| Ctrl+` | Toggle inline code (`code`) |

**Files Modified:**
- `src/engine/engine.h` - Added method declarations
- `src/engine/engine.cpp` - Added implementations and keyboard handlers
- `tests/test_keyboard_shortcuts.cpp` - Added 7 tests for new shortcuts
- `Makefile` - Added test file to build

### Home/End Keys Navigation (2026-01-14)

**Issue:** Home and End keys went to document start/end instead of line start/end

**Expected behavior (standard text editors):**
- Home: Go to start of current line
- End: Go to end of current line
- Ctrl+Home: Go to document start
- Ctrl+End: Go to document end
- Shift+Home/End: Select to line start/end
- Ctrl+Shift+Home/End: Select to document start/end

**Actual behavior (before fix):**
- Home: Went to document start (position 0)
- End: Went to document end

**Solution:**
- Modified Home key handler to use `findLineStart(cursorPos)` by default
- Modified End key handler to use `findLineEnd(cursorPos)` by default
- Added Ctrl modifier check to preserve document navigation for Ctrl+Home/End
- Fixed selection logic to save cursor position BEFORE moving (was saving AFTER)

**Files Modified:**
- `src/engine/engine.cpp` - Fixed Home/End key handlers (lines 420-458)
- `tests/test_helpers.cpp` - Implemented `setCursorPosition()` using keyboard navigation
- `tests/test_home_end_keys.cpp` - Added 8 tests for Home/End key behavior
- `Makefile` - Added new test file

### Scrollbar Click and Drag (2026-01-14)

**Issue:** Scrollbar was rendered visually but not interactive - clicks and drags had no effect.

**Expected behavior (standard scrollbars):**
- Click on thumb and drag: Scroll proportionally as thumb is dragged
- Click on track (above thumb): Jump view up to that position
- Click on track (below thumb): Jump view down to that position

**Solution:**
- Added scrollbar state variables to track dragging state and cached dimensions
- Modified `render()` to cache scrollbar dimensions for hit testing
- Added scrollbar click detection in `handleMouse()` with thumb vs track differentiation
- Added scrollbar drag handling in `handleMouseMove()` with proportional scroll calculation

**Files Modified:**
- `src/engine/engine.h` - Added scrollbar state variables
- `src/engine/engine.cpp` - Added scrollbar interaction handlers
- `tests/test_helpers.h/cpp` - Added `simulateMousePress()`, `simulateMouseMove()`, `simulateMouseRelease()`
- `tests/test_scrollbar.cpp` - Added visual test for scrollbar interaction
- `Makefile` - Added test file to build
