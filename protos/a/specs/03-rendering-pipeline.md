# Rendering Pipeline

## Purpose

The PaintTree represents the visual structure of what to draw. It answers the question: "What drawing commands produce this frame?"

## Current Problems

The display list exists but provides no value - it's constructed then immediately disassembled by a switch statement in the rasterizer. The MarkdownRenderer has become a 350-line query service. The painter imports from the parse layer to check semantic types. Four identical rect operation types exist when one with a role field would suffice. Clip operations are defined but never emitted.

The result: the display list is pure overhead, the "coordinator" does too much, and layers reach back into each other.

## Target Model

### PaintTree, Not Flat List

The painter produces a tree of PaintArtifacts, not a flat list of commands. Each PaintArtifact contains:
- Display items (the actual drawing commands for this node)
- Bounds (for culling)
- Optional clip rectangle
- Child artifacts

This structure mirrors the layout tree. A paragraph's paint artifact contains its background rect and text commands; its children (inline formatting) have their own artifacts.

### Why a Tree?

**Culling**: The rasterizer can skip entire subtrees that are outside the viewport. With a flat list, you must examine every command.

**Clipping**: Clip regions are structural (a property of the artifact) rather than stateful (push/pop commands in a sequence).

**Caching**: Unchanged subtrees can be cached. If a paragraph hasn't changed, its paint artifact is reused.

**Debugging**: The paint tree mirrors document structure, making visual debugging easier.

### No Upward Queries

The painter never asks "what type of markdown element is this?" All visual properties are already resolved in the LayoutNode's style. Background colors, text colors, debug colors - everything needed for painting is present in the layout tree.

This breaks the dependency from paint layer back to parse layer.

### Self-Contained Display Items

Each display item (DrawRect, DrawText, DrawImage, DrawLine) contains all the data needed to draw it. No pointers back to layout or document. The rasterizer receives shapes and colors, not semantic information.

### Unified Rect Type

One DrawRect type with a role field (Background, Selection, Caret, Border, Debug) replaces four separate types that had identical structure.

## What PaintArtifact Contains

- **displayItems**: Drawing commands for this node
- **bounds**: Bounding box for culling
- **clipRect**: Optional clip region (replaces push/pop)
- **children**: Child paint artifacts

## What Display Items Exist

- **DrawRect**: Rectangle with color, optional border, optional corner radius, role
- **DrawText**: Position, glyph data (pre-shaped by layout), style, color
- **DrawImage**: Destination rect, texture identifier, source rect, tint
- **DrawLine**: Start, end, color, width

## Selection and Caret

Selection highlighting and caret rendering are paint-time concerns, added as overlay artifacts.

**Selection**: Given a source range from the Engine, the painter walks the LayoutTree to find all nodes that intersect that range, computes highlight rectangles, and emits DrawRect items with Selection role.

**Caret**: The Engine provides caret position (source offset) and blink state. The painter finds the visual position via the LayoutTree and emits a DrawRect with Caret role.

**Important**: Hit-testing queries the LayoutTree, not the PaintTree. Paint decorations (selection, caret, debug boxes) don't affect click targets - the click "sees through" them to the content beneath.

## Window Resize

When the window resizes, layout recomputes and selection/caret rectangles update automatically. Selection is stored as source range (in the Engine), not screen coordinates. The visual representation recomputes from the source range after reflow.

## What Gets Deleted

- The separate DrawDebugBorderOp, DrawSelectionRectOp, DrawCaretOp types
- The SetClipOp and RestoreClipOp that were never used
- The painter stub methods (paintBorder, paintLinkUnderline, etc.)
- The 350 lines of query logic in MarkdownRenderer
- The parse layer imports from paint layer

## Success Criteria

The painter produces a tree that mirrors layout structure. Clipping is structural, not stateful. The rasterizer can cull entire subtrees. No component queries upstream layers. Debug visualization uses pre-computed colors from layout.
