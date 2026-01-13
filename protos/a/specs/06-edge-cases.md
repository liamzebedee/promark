# Hit-Testing and Interaction Edge Cases

This document defines behavior for ambiguous or edge-case interactions in the editor.

---

## Clicking on Content Types

### Images

**Behavior**: Single-click selects the entire image.

Images are atomic elements. Clicking anywhere on an image selects the whole thing, like selecting an image in a word processor. To deselect, click elsewhere. To delete, press Backspace while selected.

### Links

**Behavior**: Single-click follows the link.

Clicking a link opens/follows it immediately. To edit link text, use keyboard navigation (arrow keys to move caret into the link). There is no modifier-click alternative.

### Decorative Elements (Bullets, Blockquote Bars, List Numbers)

**Behavior**: Ignore the click.

Clicking on decorative/chrome elements does nothing. The user must click on actual content to position the caret.

### Code Blocks

**Behavior**: Fixed-width grid positioning.

Code blocks use monospace font. Click position is calculated using the fixed character width grid, which feels natural for monospace content.

### Math Equations

**Behavior**: Unimplemented.

Math rendering is not yet implemented. When implemented, equations will likely behave as atomic elements like images.

---

## Selection Behavior

### Double-Click (Word Selection)

**Behavior**: Select word plus its formatting syntax.

Double-clicking "world" in rendered `**world**` selects the entire source range including the asterisks (positions 0-9). Deleting the selection removes the formatting.

This ensures formatting is not orphaned when deleting words.

### Triple-Click (Paragraph Selection)

**Behavior**: Select current paragraph.

Triple-click selects from paragraph start to paragraph end, including any inline formatting within.

### Cross-Block Selection

**Behavior**: Select all content between start and end points.

Selection can span across different block types (paragraph into list, list into code block, etc.). The source range includes all content and syntax between the selection endpoints.

### Drag Selection Outside Document

**Behavior**: Auto-scroll and extend selection.

When drag-selecting and the mouse moves outside the visible area:
- Document auto-scrolls in the drag direction
- Selection extends as new content becomes visible

This matches standard word processor behavior.

---

## Caret Positioning

### Click Between Characters

**Behavior**: Nearest midpoint wins.

When clicking exactly at the boundary between two characters, the caret goes to whichever character's center is closer to the click point.

### Click in Empty Space

**Behavior**: Find nearest content.

- Click below document → caret at end of document
- Click above document → caret at start of document
- Click in left margin → caret at start of nearest line
- Click in right margin → caret at end of nearest line
- Click between blocks → caret at end of previous block or start of next (whichever is closer)

### Arrow Key Navigation in Visual Mode

**Behavior**: Skip hidden syntax.

Arrow keys move through visible characters only. Moving right from the space in "Hello **world**" goes directly to "w", skipping the invisible `**`.

The caret never rests on invisible syntax positions in visual mode.

---

## Line and Paragraph Navigation

### Cmd+Left / Cmd+Right

**Behavior**: Move to paragraph start/end.

These shortcuts move to the beginning or end of the current paragraph (source line), not the visual line created by word wrap.

### Home / End Keys

**Behavior**: Move to visual line start/end.

Home and End move to the start/end of the current visual (wrapped) line.

---

## Copy and Paste

### Copying Selection with Hidden Syntax

**Behavior**: Copy valid markdown with intelligent closure.

When copying a selection that spans formatted text, the clipboard receives the markdown source with syntax properly closed.

Example: Selecting "llo **wor" from "Hello **world**" copies "llo **wor**" (closing the bold syntax).

This ensures pasted content is valid markdown.

---

## Mode Switching

### Visual to Raw Mode

**Behavior**: Caret stays on same character.

When switching modes, the caret remains on the same visible character, not the same byte offset.

Example: Caret on "G" in visual mode (where source is `**G**`) moves to "G" in raw mode, which is now at a different byte offset because the `**` is visible.

### Raw to Visual Mode

Same principle: caret tracks the character, not the byte position.

---

## Undo/Redo

### Caret Restoration

**Behavior**: Restore both caret and scroll position.

Undo returns the editor to exactly the previous state: text content, caret position, and scroll position. The user sees what they saw before the undone action.

---

## Table Interaction

### Cell Selection

**Behavior**: Text selection only.

Tables support only text selection within cells. There is no cell-level or multi-cell selection. Click in a cell to position caret, drag to select text within that cell.

Tab key moves focus between cells (separate from hit-testing).

---

## Special Cases

### Emoji and Multi-Byte Characters

**Behavior**: Atomic character handling.

Emoji and other multi-byte characters are treated as single units. The caret cannot be positioned between the bytes of a single character. Clicking on an emoji positions the caret before or after it.

### Empty Lines in Source

**Behavior**: Not clickable, but navigable.

Empty lines between blocks have no visual representation and therefore no click target. However, arrow keys can navigate through these positions. The caret can exist at these positions, but the user cannot click to place it there.

### Window Resize During Selection

**Behavior**: Maintain source range.

If the window resizes while a selection is active, the selection stays on the same source range. The visual highlight rectangles recompute to match the new layout, but the selected content doesn't change.

### Window Resize During Drag

**Behavior**: Recompute layout, continue drag.

If the window resizes during a drag operation, layout recomputes and the drag continues with updated geometry. The selection adjusts to the new positions.

---

## Summary Table

| Interaction | Behavior |
|-------------|----------|
| Click image | Select entire image |
| Click link | Follow link |
| Click bullet/quote bar | Ignore |
| Double-click word | Select word + formatting |
| Triple-click | Select paragraph |
| Cmd+Left/Right | Paragraph start/end |
| Home/End | Visual line start/end |
| Arrow through syntax | Skip hidden syntax |
| Copy formatted text | Valid markdown with closed syntax |
| Mode switch | Track character, not byte |
| Undo | Restore caret + scroll |
| Click empty space | Nearest content |
| Click between chars | Nearest midpoint |
| Drag outside | Auto-scroll + extend |
| Table cells | Text selection only |
| Emoji | Atomic (no internal caret) |
