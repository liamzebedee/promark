# Bug: keyboard-navigation-long-document

## Reproduction Steps

1. Load `resources/long-post.md` into the editor
2. Place cursor at the top of the document (line 1, column 0)
3. Press the Down arrow key repeatedly until reaching the end of the document
4. Press the Up arrow key repeatedly until returning to the top of the document
5. Place cursor at the top of the document again
6. Press the Right arrow key repeatedly until reaching the end of the document
7. Press the Left arrow key repeatedly until returning to the top of the document

## Expected Behavior

- Down/Up arrow keys should navigate through every line of the document without getting stuck at any point
- Right/Left arrow keys should navigate through every character of the document without getting stuck at any point
- Navigation should be smooth and continuous - no infinite loops or positions where the cursor refuses to move
- The cursor should reach the very end of the document and return to the very beginning

## Actual Behavior

**Before fix:**
- Down arrow navigation got stuck at position 238 (start of "## Peace and work." heading)
- Up arrow navigation got stuck at position 8868 (middle of document)
- The cursor would not move past certain elements, particularly headings with larger font sizes
- Left/Right arrow navigation worked correctly through all characters

**Root Cause:**
The vertical navigation step size was calculated using `Typography::BASE_FONT_SIZE` (16px), but headings have larger font sizes (e.g., H2 is ~33px). When the cursor was on a heading, the step was too small to move past the heading's hit region, causing the cursor to stay in place.

**Fix:**
1. Added `getFontSizeAt(domPos)` method to get the actual font size at the cursor position
2. Modified `moveCursorVertically()` to use the actual font size for calculating step distance
3. Added retry logic with progressively larger steps to handle edge cases (gaps between elements)

## Severity

Medium - keyboard navigation is a core editing feature and getting stuck breaks the user experience
