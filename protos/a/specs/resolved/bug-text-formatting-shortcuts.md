# Bug: text-formatting-shortcuts

## Reproduction Steps

### Bold (Ctrl+B)
1. Load `resources/long-post.md` into the editor
2. Select some text (e.g., "return on peace" on line 12)
3. Press Ctrl+B
4. Observe the result

### Italic (Ctrl+I)
1. Select some text
2. Press Ctrl+I
3. Observe the result

### Strikethrough (Ctrl+Shift+S or similar)
1. Select some text
2. Press the strikethrough shortcut
3. Observe the result

### Code/Monospace (Ctrl+` or Ctrl+E)
1. Select some text
2. Press the code shortcut
3. Observe the result

### Link (Ctrl+K)
1. Select some text
2. Press Ctrl+K
3. Observe the result

### No Selection Cases
1. Place cursor in the middle of a word (no selection)
2. Press Ctrl+B
3. Observe - should bold the current word or insert toggleable bold markers

## Expected Behavior

**With text selected:**
- Ctrl+B: Wrap selection with `**selection**` (bold)
- Ctrl+I: Wrap selection with `*selection*` or `_selection_` (italic)
- Strikethrough: Wrap selection with `~~selection~~`
- Code: Wrap selection with `` `selection` ``
- Ctrl+K: Wrap selection as `[selection](url)` and prompt/position for URL entry

**Without text selected (cursor in word):**
- Should either bold the entire word the cursor is on, OR
- Insert paired markers `**|**` with cursor positioned between them ready to type

**Toggle behavior:**
- If selected text is already bold (`**text**`), pressing Ctrl+B should remove the bold markers
- Same toggle behavior for other formatting

## Actual Behavior

**Status: RESOLVED**

The formatting shortcuts have been implemented and tested:
- Ctrl+B correctly wraps selection with **bold** markers
- Ctrl+I correctly wraps selection with *italic* markers
- Ctrl+K correctly creates [link](url) syntax
- Ctrl+` correctly wraps selection with `inline code` markers
- All shortcuts work both with and without text selection

See IMPLEMENTATION_PLAN.md "Keyboard Shortcuts (2026-01-14)" for details.

## Severity

High - text formatting shortcuts are core editor functionality that users expect to work intuitively
