# Bug: Shift+Up Selects Entire Paragraph Instead of Line

## Reproduction Steps

1. Open `long-post.md`
2. Click to position cursor in the middle of a long paragraph (one that wraps to multiple visual lines)
3. Press Shift+Up

## Expected Behavior

Selection extends upward by one visual line - selecting from current cursor position to the same horizontal position on the line above.

## Actual Behavior

Selection jumps to select the entire paragraph block (or to the previous paragraph). Instead of line-by-line selection, it selects large chunks of text.

## Severity

High - standard text selection behavior (Shift+Arrow) is broken. Users expect line-by-line selection like every other text editor.
