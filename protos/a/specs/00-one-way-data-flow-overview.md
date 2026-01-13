# One-Way Data Flow Architecture

## The Problem

The current architecture has bidirectional dependencies, split authority, and multiple sources of truth. Data flows in circles: the painter queries back to the parser, the shell duplicates engine state, and three copies of document text exist simultaneously.

## The Solution: Three Trees

The rendering system maintains three parallel tree structures. Data flows in one direction only: down.

```
Text → DocumentTree → LayoutTree → PaintTree → Pixels
```

Each tree has a single owner that produces it. Each tree is immutable once produced. Downstream consumers cannot modify upstream data.

### DocumentTree

Produced by the **Parser** from raw text.

Represents *what the content means*: paragraphs, headings, emphasis, links. Pure semantic structure with no knowledge of screen coordinates or visual appearance.

Invalidated when: text changes.

### LayoutTree

Produced by the **LayoutEngine** from DocumentTree plus viewport constraints.

Represents *where things are*: bounding boxes, line breaks, positioned children. Resolves all styles to concrete values (font sizes, colors, spacing). Contains everything needed for hit-testing and cursor positioning.

Invalidated when: document changes, or viewport width changes.

### PaintTree

Produced by the **Painter** from LayoutTree plus view state (scroll position, selection, caret).

Represents *what to draw*: rectangles, text runs, images. Organized hierarchically so entire subtrees can be skipped if offscreen. Contains no semantic or geometric knowledge - just drawing commands.

Invalidated when: layout changes, or view state changes (scroll, selection).

## Why Three Trees?

### Independent Caching

Each tree is cached independently. Scrolling only rebuilds the PaintTree. Resizing only rebuilds Layout and Paint. Only text edits touch all three.

### Clear Responsibilities

Each component has one job:
- Parser understands markdown syntax
- LayoutEngine understands geometry
- Painter understands visual representation
- Rasterizer understands GPU

No component reaches into another's domain.

### Debuggable

You can inspect each tree independently. "Why is this text in the wrong place?" - check the LayoutTree. "Why is it the wrong color?" - check the PaintTree. "Why is it parsed wrong?" - check the DocumentTree.

### No Upward Queries

The painter never asks "what type of markdown element is this?" - that information was already resolved into style properties by the layout phase. The rasterizer never asks "where should this be positioned?" - positions are already in the paint commands.

## Single Writers

Every piece of mutable state has exactly one owner.

| Data | Owner | Readers |
|------|-------|---------|
| Raw text content | Engine (TextModel) | Parser |
| Document structure | Parser | LayoutEngine |
| Positioned geometry | LayoutEngine | Painter |
| Drawing commands | Painter | Rasterizer |
| Cursor position | Engine | Painter (via ViewState) |
| GPU resources | Rasterizer | None |

The shell doesn't track dirty state - it asks the engine. The painter doesn't store layout data - it receives it fresh each frame. The rasterizer doesn't know about markdown - it just draws shapes.

## Immutable Handoffs

When data crosses a boundary, it's handed off as an immutable snapshot. The parser cannot modify the text it receives. The layout engine cannot modify the document tree. The painter cannot modify the layout tree.

This eliminates an entire class of bugs: "who changed this data?" has exactly one answer.

## Hit-Testing: Reading the Trees

Hit-testing (click → source position) is a read-only query on immutable data. It doesn't violate one-way flow because nothing is mutated.

The query flows: Screen coordinates → LayoutTree → source position.

The LayoutTree is the right place for hit-testing because it knows both geometry (where things are) and source positions (where they came from). The PaintTree only knows drawing commands. The DocumentTree doesn't know screen positions.

See [06-edge-cases.md](06-edge-cases.md) for detailed hit-testing behavior.
