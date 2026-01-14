# Bug: Keyboard Shortcuts Don't Work Fully

## Reproduction Steps

1. Open the editor with some text
2. Select text
3. Try various keyboard shortcuts - some work, others don't

## Current Working Shortcuts (from `src/engine/engine.cpp:218-275`)

| Shortcut | Action | Status |
|----------|--------|--------|
| Ctrl+W | Close window | Works |
| Ctrl+A | Select all | Works |
| Ctrl+C | Copy selection | Works |
| Ctrl+V | Paste | Works |
| Ctrl+Z | Undo | Works |
| Ctrl+Shift+Z | Redo | Works |
| Ctrl+Y | Redo (alternative) | Works |
| Ctrl+R | Toggle raw/visual mode | Works |
| Ctrl+S | Save file (in edit.cpp:196) | Works |
| Ctrl+Left | Jump to line start | Works |
| Ctrl+Right | Jump to line end | Works |
| Ctrl+Up | Jump to document start | Works |
| Ctrl+Down | Jump to document end | Works |
| Ctrl+Backspace | Delete to line start | Works |
| Alt+Left | Move by word backward | Works |
| Alt+Right | Move by word forward | Works |
| Escape | Cancel selection | Works |

## Missing Shortcuts

| Shortcut | Action | Priority |
|----------|--------|----------|
| Ctrl+X | Cut selection | High |
| Ctrl+B | Toggle bold (**text**) | High |
| Ctrl+I | Toggle italic (*text*) | High |
| Ctrl+K | Insert link [text](url) | Medium |
| Ctrl+` | Toggle inline code | Medium |
| Ctrl+Shift+K | Toggle strikethrough | Low |

## Expected Behavior

All standard text editor shortcuts should work, plus markdown-specific formatting shortcuts that wrap selected text with appropriate syntax.

## Actual Behavior

Cut (Ctrl+X) is missing entirely. Formatting shortcuts (bold, italic, etc.) are not implemented.

## Severity

Medium - Core clipboard operations missing Ctrl+X. Formatting shortcuts are a quality-of-life expectation.

## Implementation Notes

1. Add `cutSelection()` method similar to `copySelection()` but also deletes the text
2. Add formatting toggle functions that:
   - If text is selected: wrap/unwrap with markdown syntax
   - If no selection: insert syntax pair and place cursor between
3. Add tests for each new shortcut in `tests/test_keyboard_shortcuts.cpp`
