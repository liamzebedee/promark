# Bug: Caret Position Incorrect After Typing

**Status: RESOLVED**

## Reproduction Steps

1. Open `long-post.md`
2. Click anywhere in the document to place cursor
3. Type some text (e.g., "hello")
4. Press Enter to insert newline
5. Type more text (e.g., "world")

## Expected Behavior

Caret appears at the correct position after each keystroke - immediately after the character just typed, or at the start of the new line after Enter.

## Actual Behavior

- After pressing Enter, caret appears in top-left area of the document
- After typing subsequent characters, caret is incorrectly positioned
- Caret seems to "jump around" rather than staying where text is being inserted

## Severity

High - fundamental text editing UX is broken. User cannot see where they are typing.

## Root Cause Analysis

The caret uses an animated position (`caretAnimX/Y`) that lerps toward a target position. The bug occurred because the render loop updated the animation target AFTER painting the caret:

**Original render order in `Engine::render()` (buggy):**
1. `setCaretState()` - uses OLD `caretAnimX/Y` values
2. `markdownRenderer->render()` - paints caret at OLD position
3. `updateCaretAnimation()` - computes NEW target, lerps toward it

This meant the painted caret was always one frame behind the actual cursor position.

## Relevant Code Locations

- `src/engine/engine.cpp:140-172` - Main render loop
- `src/engine/engine.cpp:884-914` - `updateCaretAnimation()`
- `src/engine/painter.cpp:408-467` - `paintCaret()` - uses animated position when enabled
- `src/engine/markdown_renderer.cpp:430-479` - `getCursorXY()` - computes target from layout

## Solution

Two fixes were applied:

1. **Reordered render loop** (`engine.cpp:140-172`):
   - Added `ensureLayoutValid()` call BEFORE updating animation
   - Moved `updateCaretAnimation()` BEFORE `setCaretState()`
   - Now the caret is painted with the correct animated position

2. **Snap on large movements** (`engine.cpp:884-914`):
   - Added distance threshold (20 pixels) to `updateCaretAnimation()`
   - If target moved significantly, snap immediately instead of lerping
   - Prevents visible lag when cursor jumps to new position

Added `ensureLayoutValid()` method to `MarkdownRenderer` (`markdown_renderer.cpp:88-109`) to force layout computation without painting, enabling accurate cursor position calculation before rendering.

## Test Coverage

Visual tests added in `tests/test_bug_caret_after_typing.cpp`:
- `bug_caret_position_after_enter` - Verifies caret position after Enter key
- `bug_caret_position_during_typing` - Verifies caret tracks typed characters
- `bug_caret_enter_in_middle` - Verifies Enter in middle of paragraph
