# Bug: caret-position-mode-switch

## Reproduction Steps

1. Load `resources/long-post.md` into the editor
2. In visual mode, place the caret at a specific position (e.g., middle of a word on line 12)
3. Note the exact character position
4. Switch to raw mode
5. Observe where the caret lands
6. Switch back to visual mode
7. Observe if the caret returns to the original position

Also test:
- Caret at start of line
- Caret at end of line
- Caret inside formatted text (bold, italic, links)
- Caret inside blockquotes
- Caret in headings

## Expected Behavior

- Caret should maintain its logical document position when switching modes
- If caret is after character N in visual mode, it should be after character N in raw mode (accounting for hidden markdown syntax)
- Round-trip switching (visual → raw → visual) should return caret to exact original position

## Actual Behavior

- Caret position is close but not exact when switching between raw and visual modes
- (Document specific offset patterns observed - e.g., always off by 1, off by length of markdown syntax, etc.)

## Possible Causes

- Off-by-one in position translation logic
- Not accounting for collapsed/hidden markdown syntax characters
- Different handling of line endings
- Unicode/grapheme cluster boundary differences between modes

## Severity

Low - caret is close enough to be usable, but imprecision is noticeable
