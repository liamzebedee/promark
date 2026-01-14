# Test: Long User E2E Test

## Purpose

Comprehensive visual end-to-end test simulating a user creating a complex document. Verifies the editor works correctly across a realistic usage session.

## Test Sequence

### 1. Document Creation (fast typing, not human speed)

Type the following document structure:

```markdown
# My Project Notes

## Introduction

This is a paragraph with **bold text**, *italic text*, and ***bold italic*** combined.

Here's a [link to docs](https://example.com) and some `inline code`.

## Code Examples

```python
def hello_world():
    print("Hello, World!")
    return True
```

## Quotes and Lists

> This is a block quote
> spanning multiple lines

- First item
- Second item with **bold**
  - Nested item
  - Another nested
- Third item

1. Numbered one
2. Numbered two
3. Numbered three

## Table

| Name | Value | Description |
|------|-------|-------------|
| foo  | 123   | A foo thing |
| bar  | 456   | A bar thing |

## Final Notes

~~Strikethrough text~~ and regular text mixed together.

The end.
```

### 2. Screenshot Checkpoints

Take screenshots after:
- [ ] Initial empty state
- [ ] After typing header and intro paragraph
- [ ] After adding code block
- [ ] After adding quote and lists
- [ ] After adding table
- [ ] After completing full document
- [ ] After scrolling to top
- [ ] After scrolling to bottom
- [ ] After toggling to raw mode (Ctrl+R)
- [ ] After toggling back to visual mode

### 3. Interaction Tests (with screenshots)

- [ ] Click in middle of paragraph, verify cursor position
- [ ] Select text with mouse drag, verify highlight
- [ ] Use Ctrl+A to select all, verify full selection
- [ ] Copy/paste a section
- [ ] Undo (Ctrl+Z) and verify state
- [ ] Redo (Ctrl+Y) and verify state
- [ ] Navigate with arrow keys through different blocks
- [ ] Click inside code block, verify cursor
- [ ] Click inside table cell, verify cursor
- [ ] Click inside nested list, verify cursor

### 4. Visual Verification Criteria

Each screenshot should verify:
- Text renders without overlap or clipping
- Formatting (bold/italic) displays correctly
- Code blocks have distinct styling
- Block quotes are visually indented
- Lists have proper bullets/numbers and indentation
- Table renders with visible structure
- Cursor/caret is visible and positioned correctly
- Selection highlighting is visible when active
- Scroll position matches expected view

## Expected Behavior

All operations complete without crashes, visual glitches, or incorrect rendering. The document should look like a properly formatted markdown preview throughout.

## Severity

Critical - This is the core user experience test.
