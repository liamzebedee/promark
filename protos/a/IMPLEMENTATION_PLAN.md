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

### Caret and Selection in Blockquotes (2026-01-14)

**Issue:** Caret and selection highlights were not visible inside blockquotes, making it difficult to edit blockquote content.

**Root Cause:** The rasterizer was drawing display items in incorrect z-order:
1. Selection and caret were drawn first (from root artifact's displayItems)
2. Then children (content tree including blockquote backgrounds) were drawn on top

This caused blockquote backgrounds to cover both selection highlights and the caret.

**Solution:**
Changed rasterizer to draw items in correct z-order:
1. Non-selection, non-caret items from artifact
2. Recursively process children (content including backgrounds)
3. Selection (after backgrounds, visible on top)
4. Caret (last, on top of everything)

**Files Modified:**
- `src/engine/rasterizer.cpp` - Fixed render order for selection/caret
- `tests/test_blockquote_caret.cpp` - Added 2 tests for blockquote caret/selection
- `Makefile` - Added test file to build

### Keyboard Navigation in Long Documents (2026-01-14)

**Issue:** Up/Down arrow keys got stuck at certain positions when navigating long documents, particularly around headings.

**Root Cause:** The vertical navigation step size was calculated using `Typography::BASE_FONT_SIZE` (16px), but headings have larger font sizes (e.g., H2 is ~33px). When the cursor was on a heading, the step was too small to move past the heading's hit region.

**Solution:**
- Added `getFontSizeAt(domPos)` method to MarkdownRenderer to get the actual font size at cursor position
- Modified `moveCursorVertically()` to use actual font size for step calculation
- Added retry logic with progressively larger steps to handle edge cases (gaps between elements)

**Files Modified:**
- `src/engine/markdown_renderer.h` - Added `getFontSizeAt()` declaration
- `src/engine/markdown_renderer.cpp` - Implemented `getFontSizeAt()`
- `src/engine/engine.h` - Added `getCursorPosition()` method for testing
- `src/engine/engine.cpp` - Fixed `moveCursorVertically()` to use actual font size
- `tests/test_long_document_navigation.cpp` - Added 4 tests for navigation
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
