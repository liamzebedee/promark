# Bug: raw-mode-click-empty-lines

## Reproduction Steps

1. Load `resources/long-post.md` into the editor
2. Switch to raw mode
3. Try to click on an empty line (e.g., line 7 between the heading and paragraph)
4. Observe that the click does not place the caret on that line

## Expected Behavior

- Clicking on an empty line should place the caret at column 0 of that line
- Empty lines should be valid click targets just like any other line
- This is basic text editor functionality

## Actual Behavior

- Cannot click on empty lines in raw mode
- Click is ignored or caret jumps to a different line

## Root Cause Investigation

- Empty lines may have zero height in hit-testing
- Line hit regions may only cover actual text content, not the full line height
- Missing click handler for whitespace/empty regions

## Fix Requirements

- Every line (including empty lines) must have a clickable region
- Line height should be consistent whether the line has content or not
- Hit-testing should map clicks in empty space to the nearest valid caret position

## Severity

High - this breaks basic text editing workflow
