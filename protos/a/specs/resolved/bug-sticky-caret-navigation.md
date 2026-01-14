# Bug: Sticky Caret Navigation

## Reproduction Steps

1. Navigate through the document using arrow keys
2. Move up/down and left/right through text
3. Caret gets "stuck" in weird positions

## Expected Behavior

Simple, naive caret navigation - just move to the next/previous character or line without any special logic.

## Actual Behavior

Caret navigation is "sticky" - it gets stuck in weird places, likely due to over-engineered positioning logic.

## Severity

High - core editing UX is broken
