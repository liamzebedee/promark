# Bug: Home/End Keys Navigate to Document Instead of Line

## Reproduction Steps

1. Open a document with multiple lines
2. Position cursor in the middle of any line (not the first or last line)
3. Press Home key
4. Observe cursor position

## Expected Behavior

**Home key** should move cursor to the START OF THE CURRENT LINE (like VS Code, Sublime, macOS TextEdit):
- First press: Go to first non-whitespace character on line (optional, some editors skip this)
- Or simply: Go to column 0 of the current line

**End key** should move cursor to the END OF THE CURRENT LINE:
- Go to position right after last character on current line (before newline)

**Ctrl+Home** should go to document start (position 0)
**Ctrl+End** should go to document end (after last character)

**Shift+Home** should select from cursor to line start
**Shift+End** should select from cursor to line end

## Actual Behavior

- Home key jumps to DOCUMENT START (position 0)
- End key jumps to DOCUMENT END (after last character)

This is broken because:
1. Users expect standard text editor behavior
2. There's no way to quickly navigate to line start/end
3. Ctrl+Left/Right already exist for word navigation, but Home/End is fundamental

## Code Location

`src/engine/engine.cpp` lines 420-435:
```cpp
} else if (key == GLFW_KEY_HOME) {
    cursorPos = 0;  // BUG: Should be line start, not document start
    ...
} else if (key == GLFW_KEY_END) {
    cursorPos = static_cast<int>(textBuffer->getLength());  // BUG: Should be line end
    ...
}
```

## Severity

High - This is fundamental text editing UX that every user expects to work.
