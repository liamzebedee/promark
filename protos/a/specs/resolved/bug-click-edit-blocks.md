# Bug: Verify Click and Edit Works Inside Block Elements

## Reproduction Steps

1. Open a document containing various block elements
2. Try clicking to place cursor inside each block type:
   - Tables (click on a cell)
   - Block quotes (> quoted text)
   - Code blocks (```fenced code```)
   - Nested lists
   - Headers
3. Try editing text at the clicked position
4. Try selecting text within these blocks

## Expected Behavior

- Clicking inside any block element should place the cursor at that position
- Typing should insert text at the cursor position
- Selection should work normally within blocks
- Cursor position should map correctly between visual and raw modes

## Actual Behavior

**VERIFIED WORKING (2026-01-14)**: Click-to-edit works correctly inside all block elements:
- Code blocks: Text inserted at clicked position
- Block quotes: Text inserted at clicked position
- Nested lists: Text inserted at clicked position
- Headers: Text inserted at clicked position

Visual tests added in `tests/test_click_edit_blocks.cpp` confirm functionality.

## Severity

High - Inability to edit inside common block elements would severely limit editor usability.
