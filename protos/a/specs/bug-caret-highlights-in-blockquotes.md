# Bug: caret-highlights-in-blockquotes

## Reproduction Steps

1. Load `resources/long-post.md` into the editor
2. Navigate cursor into a blockquote section (e.g., lines 18-20, 48-50, or 96-100 which contain `>` prefixed text)
3. Observe the caret visibility
4. Select text within the blockquote by shift+arrow keys or click-drag
5. Observe the selection highlight visibility

## Expected Behavior

- The caret (blinking cursor) should be visible when positioned inside blockquote blocks
- Text selection highlights should be visible when selecting text within blockquotes
- Visual feedback should be consistent with how caret/highlights appear in regular paragraph text

## Actual Behavior

- Caret does not appear when cursor is inside a blockquote
- Selection highlights do not show when selecting text within blockquotes

## Severity

High - users cannot see where they are typing or what they have selected inside blockquotes, making editing quote content very difficult
