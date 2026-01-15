# Bug: List Items and Tables Overlap in Visual Mode

## Summary

When a markdown document contains lists followed by tables, the visual mode rendering shows severe layout overlap. List items render on top of table headers, and table cells appear disconnected.

## Reproduction

Create a document with:
```markdown
- First item
- Second item
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
```

## Expected Behavior

- Each list item should be on its own line with proper vertical spacing
- Numbered list should appear after bullet list with appropriate block spacing
- Table should appear after numbered list with appropriate block spacing
- No elements should overlap

## Actual Behavior

In the E2E test screenshots:
- "1. Numbered one" overlaps with "▪ Another nested"
- "2. Numbered two" overlaps with "▪ Third item"
- "3. Numbered three" overlaps with "## Table" heading
- Table header "Col A" overlaps with "▪ List item two"
- Table cells appear disconnected from the table structure

## Screenshots

See `/tmp/promark_tests/e2e_04_scroll_bottom_*.png` and `/tmp/promark_tests/e2e_interact_10_click_list_*.png`

## Hypothesis

The layout engine may be:
1. Not properly advancing Y position after nested list items
2. Calculating incorrect heights for list/table blocks
3. Having issues with the transition between different block types

## Severity

Critical - core document rendering is broken for common markdown structures.
