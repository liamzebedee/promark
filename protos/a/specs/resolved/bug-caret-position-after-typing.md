# Bug: Caret Position Incorrect After Typing

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
