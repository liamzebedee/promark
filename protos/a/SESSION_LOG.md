# Session Log

This file tracks work sessions to detect circular patterns and prevent endless loops.

---

## Session: 2026-01-14

### Current Bug: `specs/bug-visual-mode-collapse-empty-lines.md`

**Bug Summary:** Empty lines between paragraphs in visual mode should be collapsed (hidden), like browsers render markdown. Currently they render as visible blank lines, doubling the spacing.

### Work Attempts

| # | Approach | Result | Notes |
|---|----------|--------|-------|
| 1 | Investigate existing collapsing logic | **Already Fixed** | Code exists in layout_engine.cpp:170-202 |
| 2 | Measure vertical gaps programmatically | **Confirmed Working** | Gap = 30.4px (expected: 30.4px with collapsing) |
| 3 | Visual screenshot comparison | **Looks Correct** | Visual mode collapses, raw mode shows empty lines |

### Findings

The bug was already fixed by previous work. The collapsing logic in `layout_engine.cpp` (lines 170-202):
- Detects empty paragraphs (children all have empty text)
- Sets their height to 0
- Skips Y position advancement
- Does not add block spacing after collapsed elements

Test measurements confirmed:
- Single empty line: 30.4px gap (correct)
- Multiple empty lines (3): 30.4px gap (all collapsed to 0)

The expected gap formula: `16px * 1.4 (line height) + 8px (block spacing) = 30.4px`

---

## Resolved This Session

### `bug-visual-mode-collapse-empty-lines.md` - Already Fixed

**Status:** Bug was already resolved prior to investigation. Collapsing logic exists and works correctly.

---

## Notes & Learnings

- 18 bugs resolved and moved to `specs/resolved/`
- All tests passing (104 tests)

---

---

## Session: 2026-01-14 (continued)

### Current Bug: `specs/bug-link-inside-bold.md`

**Bug Summary:** When a markdown link is placed inside bold text like `**Bold [link](url)**`, the link syntax was shown literally instead of being rendered as a clickable link.

### Work Attempts

| # | Approach | Result | Notes |
|---|----------|--------|-------|
| 1 | Investigate parser structure | Found root cause | `parseInlineElements` doesn't support nested formatting |
| 2 | Switch paragraphs to use `createInlineChildren` | Layout broke | Tree children caused vertical stacking |
| 3 | Use tree parsing but flatten to single Text child | **Success** | Parse with tree, extract annotations, clear children, add single Text |

### Root Cause Analysis

The regular paragraph parsing used `parseInlineElements()` which extracts display text and adds style/link ranges but does NOT recursively parse nested formatting. When it found `**bold content**`, it simply extracted the content as a flat string without parsing links inside.

The newer `createInlineChildren()` function properly supports recursive nested formatting but creates a tree of child nodes (Strong, Link, Text). The layout system expected paragraphs to have a single Text child, not a tree.

### Solution

1. Use `createInlineChildren()` for tree-based parsing (supports nested formatting)
2. Call `collectDisplayText()` to get flattened text
3. Call `buildStyleRangesFromTree()` to derive style/link annotations
4. Clear the tree children from the paragraph
5. Add a single Text child with the flattened display text

### Files Modified

- `src/engine/markdown_parser.cpp` - Updated regular paragraph parsing to use tree-based model then flatten
- `src/engine/markdown_objects.h` - Added `clearChildren()` method
- `tests/test_link_in_bold_debug.cpp` - Added debug tests for nested formatting

---

## Resolved This Session

### `bug-link-inside-bold.md` - Fixed

**Status:** Fixed. Nested inline formatting (link inside bold, bold inside link) now works correctly.

---

## Notes & Learnings

- 19 bugs resolved and moved to `specs/resolved/`
- All tests passing (107 tests)
- Key insight: Parser tree structure is for parsing, layout expects flattened Text children

---

## Session Status: COMPLETE

**Exit condition met:** All bugs in `specs/bug-*.md` have been fixed and moved to `specs/resolved/`.

No unresolved bug spec files remain. The codebase is stable with all tests passing.

---

## Session: 2026-01-15

### Current Bug: `specs/bug-list-table-overlap.md`

**Bug Summary:** Lists and tables in visual mode had severe layout overlap. List items rendered on top of table headers, numbered lists overlapped with bullet lists, and elements following lists were positioned incorrectly.

### Work Attempts

| # | Approach | Result | Notes |
|---|----------|--------|-------|
| 1 | Run E2E tests and visually inspect | Bug confirmed | Screenshots showed clear overlap |
| 2 | Add debug output to trace Y positions | Positions correct at root | Issue in position propagation |
| 3 | Fix: Only propagate positions from root | **Success** | Removed double-propagation |

### Root Cause Analysis

The `propagatePositionToChildren()` function was being called multiple times for nested containers:

1. When List's `layoutBlockFlow()` processed its ListItem children, it called `propagatePositionToChildren()` to set their positions relative to the List
2. Then when Document's `layoutBlockFlow()` processed the List, it called `propagatePositionToChildren()` again, which recursively updated all descendants
3. This caused child positions to be double-offset (the Y position within the container was added twice)

Same issue affected Table layout - `layoutTable()` was calling `propagatePositionToChildren()` for table rows.

### Solution

Only call `propagatePositionToChildren()` from the root Document level. Nested containers (Lists, BlockQuotes, Tables) should not propagate positions themselves - the root's recursive propagation handles all descendants.

**Changes:**
1. Guard `propagatePositionToChildren()` calls in `layoutBlockFlow()` with `if (isRoot)`
2. Remove `propagatePositionToChildren()` call from `layoutTable()`
3. Guard empty paragraph propagation with `if (isRoot)`

### Files Modified

- `src/engine/layout_engine.cpp` - Fixed position propagation (3 locations)
- `tests/test_list_layout_debug.cpp` - Added debug tests for list layout
- `Makefile` - Added new test file

---

## Resolved This Session

### `bug-list-table-overlap.md` - Fixed

**Status:** Fixed. Lists, tables, and all block elements now position correctly without overlap.

---

## Notes & Learnings

- 20 bugs resolved and moved to `specs/resolved/`
- All tests passing (113 tests)
- Key insight: Position propagation should only happen once from the root, not from every container level
