# Implementation Plan

## Resolved Issues

### Punctuation Spacing Around Formatted Text (2026-01-14)

**Issue:** Punctuation and spaces between formatted text segments were not rendering correctly. For example, "**bold text**, *italic text*" would show "bold textitalic text" without the comma and space.

**Root Cause:** Layout engine used regular font metrics (`getGlyphAdvance`) but the rasterizer used style-specific font faces (bold, italic, bolditalic). Bold/italic fonts have different character widths than the regular font, causing a mismatch between calculated positions and rendered positions.

**Solution:**
1. Created `text_style.h` - Extracted `TextStyle` enum to shared header to avoid circular dependencies
2. Added `getGlyphAdvanceStyled()` method to `FontProvider` interface - Returns glyph width for styled fonts
3. Updated `FreeTypeFontProvider` to support all font faces (regular, bold, italic, bolditalic, mono)
4. Modified `Engine` to load styled font files (NotoSans-Bold.ttf, NotoSans-Italic.ttf, NotoSans-BoldItalic.ttf)
5. Updated `shapeText()` in layout_objects.cpp to use `getGlyphAdvanceStyled()` with per-character style mapping

**Files Modified:**
- `src/engine/text_style.h` (NEW) - Shared TextStyle enum definition
- `src/engine/font_provider.h` - Added `getGlyphAdvanceStyled()` virtual method
- `src/engine/freetype_font_provider.h/cpp` - Added styled font support and new constructor
- `src/engine/markdown_objects.h` - Changed to include text_style.h
- `src/engine/layout_objects.cpp` - Updated `shapeText()` to use styled metrics
- `src/engine/engine.h/cpp` - Added faceBold, faceItalic, faceBoldItalic members and loading
- `tests/test_formatting_spacing.cpp` (NEW) - Added visual tests for formatting spacing
- `tests/test_parser_debug.cpp` (NEW) - Added debug tests for style range verification
- `tests/test_e2e_comprehensive.cpp` (NEW) - Added comprehensive E2E formatting tests
- `Makefile` - Added new test files

**Result:** Character positions now match actual rendered positions regardless of font style, fixing spacing around formatted text.

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

### Typography Line Height (2026-01-14)

**Issue:** Text appeared cramped and harder to read compared to browser-rendered text. Line height was too tight (1.0x font size).

**Root Cause:** `Typography::LINE_HEIGHT_RATIO` was set to 1.0f, meaning line height equaled font size. Standard readability guidelines recommend 1.4-1.5x line height for body text.

**Solution:**
- Changed `LINE_HEIGHT_RATIO` from 1.0 to 1.4
- Updated all line height calculations across the codebase to use `fontSize * Typography::LINE_HEIGHT_RATIO`
- Fixed test that depended on old line height calculations

**Files Modified:**
- `src/engine/typography.h` - Changed LINE_HEIGHT_RATIO from 1.0f to 1.4f
- `src/engine/painter.cpp` - Updated 3 lineHeight calculations to use ratio
- `src/engine/markdown_renderer.cpp` - Updated 4 lineHeight calculations to use ratio
- `src/engine/layout_objects.cpp` - Updated 2 lineHeight calculations to use ratio
- `src/engine/font_provider.h` - Updated comment
- `src/engine/freetype_font_provider.cpp` - Updated comment
- `tests/test_bug_caret_after_typing.cpp` - Adjusted Y coordinate for line height change

**Result:** Body text now has ~22.4px line height (16px * 1.4), making it more comfortable to read.

### Empty Line Collapsing in Visual Mode (2026-01-14)

**Issue:** Bug report claimed empty lines between paragraphs were rendered as visible blank lines in visual mode, doubling spacing compared to browser rendering.

**Investigation Result:** Bug was already fixed by prior implementation.

**Existing Solution (in layout_engine.cpp:170-202):**
1. Detects empty paragraphs (children all have empty text)
2. Sets their height to 0 (collapsed)
3. Skips Y position advancement
4. Does not add block spacing after collapsed elements

**Verification:**
- Single empty line between paragraphs: Gap = 30.4px (correct)
- Multiple consecutive empty lines: Gap = 30.4px (all collapsed)
- Expected formula: `16px * 1.4 (line height ratio) + 8px (block spacing) = 30.4px`
- Visual screenshots confirm proper collapsing in visual mode vs visible empty lines in raw mode

**Files Involved:**
- `src/engine/layout_engine.cpp` - Empty paragraph collapsing logic
- `tests/test_visual_mode_empty_lines.cpp` - Added measurement assertions

**Result:** No changes required. Feature already working correctly. Added test assertions to verify behavior.

### Nested Inline Formatting (2026-01-14)

**Issue:** Links inside bold text (e.g., `**Bold [link](url)**`) showed raw markdown syntax instead of rendering the link.

**Root Cause:** Regular paragraphs used `parseInlineElements()` which doesn't recursively parse nested formatting. When it found `**bold content**`, it extracted content as a flat string without parsing links inside. The newer `createInlineChildren()` supports recursive parsing but creates tree children that the layout system couldn't handle.

**Solution:**
1. Use `createInlineChildren()` for tree-based parsing (supports nested formatting)
2. Call `collectDisplayText()` to get flattened display text
3. Call `buildStyleRangesFromTree()` to derive style/link annotations on the paragraph
4. Clear the tree children (Strong, Link, Text nodes)
5. Add a single Text child with the flattened display text

This approach combines the recursive parsing capability of the tree model with the flat layout structure the rendering system expects.

**Files Modified:**
- `src/engine/markdown_parser.cpp` - Updated regular paragraph parsing (lines 978-1001 and 964-986)
- `src/engine/markdown_objects.h` - Added `clearChildren()` method
- `tests/test_link_in_bold_debug.cpp` (NEW) - Added debug tests for nested formatting

**Result:** Nested inline formatting now works correctly:
- Link inside bold: `**Bold [link](url)**` → "Bold" in bold, "link" as bold+link
- Bold inside link: `[link **bold**](url)` → "link" as link, "bold" as bold+link
- All 107 tests passing
