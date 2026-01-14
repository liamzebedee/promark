# Bug: caret-blink-reset-on-click

## Reproduction Steps

1. Load `resources/long-post.md` into the editor
2. Wait for the caret to be in its "off" (invisible) phase of the blink cycle
3. Click somewhere else in the document
4. Observe how long it takes for the caret to appear at the new position

## Expected Behavior

- Clicking should immediately reset the blink timer and show the caret in its "on" (visible) state
- The caret should appear instantly at the click location with no perceived delay
- This matches Microsoft Word, VS Code, and other text editors
- The blink cycle restarts from the "on" phase after any user interaction (click, keypress, etc.)

## Actual Behavior

- After clicking, the caret may remain invisible until the next scheduled "on" phase of the blink cycle
- This creates a brief moment where the user doesn't see where they clicked
- Makes the editor feel laggy/unresponsive even though the position is correct

## Implementation Notes

On click (and likely on any caret-moving action):
1. Set caret to visible state immediately
2. Reset the blink timer to 0
3. Resume normal blink cycle from that point

## Severity

Low - minor polish issue but affects perceived responsiveness
