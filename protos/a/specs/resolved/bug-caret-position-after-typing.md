# Bug: Caret Position Incorrect After Typing

## Reproduction Steps

1. Open `long-post.md`
2. Click anywhere in the document to place cursor
3. Type some text (e.g., "hello this is a big fat whatever")
4. Move caret back two words or so.
4. Press Enter to insert newline
5. Type more text (e.g., "world")
6. Press Enter to insert newline

## Expected Behavior

Caret appears at the correct position after each keystroke - immediately after the character just typed, or at the start of the new line after Enter.

## Actual Behavior

Caret is in the wrong place at multiple points.

## Severity

High - fundamental text editing UX is broken. User cannot see where they are typing.
