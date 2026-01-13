# Rendering Pipeline: Architectural Summary

Synthesized from `markdown_renderer.md`, `painter.md`, `paint_operations.md`.

---

## Core Architecture

The rendering pipeline follows Parse -> Layout -> Paint -> Rasterize, with MarkdownRenderer as facade coordinator. PaintOperations defines an intermediate display list vocabulary consumed by the Rasterizer.

**Actual data flow:**
```
TextBuffer -> MarkdownParser -> objectTree
objectTree -> LayoutEngine  -> layoutTree
layoutTree -> Painter       -> DisplayList
DisplayList -> Rasterizer   -> screen
```

---

## Cross-Cutting Concerns

### 1. Inverted Dependencies (Circular Awareness)

Three modules violate the intended layering:

| Module | Imports From | Violation |
|--------|--------------|-----------|
| `paint_operations.h` | `markdown_objects.h` | Paint layer depends on parse layer for `TextStyle` enum |
| `painter.cpp` | `markdown_renderer.h` | Lower layer imports from owner for `CaretState` |
| `markdown_renderer.h` | `<ft2build.h>` | Facade exposes FreeType types in API |

**Root cause:** Shared types (`CaretState`, `TextStyle`) have no dedicated home. They were defined where first needed rather than in a shared header.

**Pattern:** Each cross-layer type creates bidirectional coupling. The codebase has 3 such types, creating 3 dependency inversions.

### 2. Query Logic Sprawl

MarkdownRenderer accumulates 350+ lines of query logic (hit-testing, position mapping, cursor queries) that bypasses the pipeline it coordinates. This logic:

- Directly traverses both `objectTree` and `layoutTree`
- Uses `dynamic_cast` chains to access type-specific methods
- Duplicates tree traversal patterns across multiple helpers: `collectTextObjects()`, `collectTextLayoutsWithPos()`

**Tension:** The renderer is nominally a pipeline coordinator but actually serves as an ad-hoc query service. These are distinct responsibilities.

### 3. Semantic Type Leakage into Paint

Painter queries `MarkdownObjectType` to make rendering decisions:

```cpp
if (layoutObject->getSourceObject()->getType() == MarkdownObjectType::BlockQuote)
    paintBlockQuoteBar(...);
```

Paint should respond to visual properties (colors, borders) declared on layout objects, not derive them from document semantics. Currently, style information is reconstructed at paint-time rather than computed during layout.

### 4. Layout Constants in Multiple Layers

`Typography::LIST_INDENT` and `Typography::BASE_FONT_SIZE` are used in both layout and paint for position calculations. The Painter computes marker positions using these constants, duplicating layout logic.

---

## Unstable Boundaries

### MarkdownRenderer: Coordinator vs Query Service

The facade has two roles with different lifecycles:
- **Pipeline coordination** (~100 lines): Stable, driven by dirty flags
- **Query service** (350+ lines): Growing, reached directly into trees

No clear contract exists for query result validity. Calling `getCursorXY()` after text changes but before `render()` returns stale data silently.

### CaretState: Renderer vs Painter vs Rasterizer

Caret handling spans three modules:
- **Definition:** `markdown_renderer.h` (lines 12-21)
- **Geometry computation:** `Painter::paintCaret()`
- **Visibility decision:** `Rasterizer::rasterize()` receives `caretVisible` bool

The visibility flag affects both paint-time (what to include) and raster-time (whether to draw), creating an undocumented contract.

Animated caret fields (`animatedCaretX`, `animatedCaretY`, `useAnimatedPosition`) are defined in CaretState but never read by MarkdownRenderer. External code sets them; painter/rasterizer consume them. Split responsibility is undocumented.

### Position Mapping: Raw vs DOM

The dual-coordinate system (Raw = original markdown, DOM = rendered text) is implemented via:
- `domToRaw()` / `rawToDOM()` in MarkdownRenderer
- `MarkdownObject::getRawStart()` / `getRawEnd()` from parser

The renderer reimplements tree traversal that the parser already performed. `getTotalDOMLength()` is computed by renderer but is logically a property of the object tree.

---

## Paper Abstractions

### Dirty Flag Optimization (Incomplete)

Three flags exist (`needsReparse`, `needsRelayout`, `needsRepaint`) but:
- `rasterize()` runs unconditionally every frame - no `needsRasterize`
- Height changes trigger relayout despite markdown layout depending only on width

### Clipping Infrastructure (Unused)

`SetClipOp` and `RestoreClipOp` are defined but never emitted. The clip stack semantics are implicit, not enforced. Rasterizer maintains clip state externally while the paint operation layer only declares intent.

### Dead/Stub Methods in Painter

| Method | Status | Evidence |
|--------|--------|----------|
| `paintBorder()` | Stub | Comment: "not yet implemented" |
| `paintLinkUnderline()` | Dead | Never called; underlines painted inline in `paintText()` |
| `isInsideLink()` | Dead | Link detection uses range-based approach instead |
| `paintTableCell()` | Stub | Comment: "could be used if needed" |
| `computeXForOffset()` | Legacy | Uses hardcoded monospace approximation despite proper glyph metrics existing |

### Display List Type Proliferation

Four structurally-identical rect operations exist:
- `DrawRectOp` - backgrounds
- `DrawDebugBorderOp` - development visualization
- `DrawSelectionRectOp` - selection highlighting
- `DrawCaretOp` - cursor

Type discrimination without behavioral difference. A single rect type with role enum would suffice.

### Inheritance + Type Enum Dual Dispatch

`PaintOp` uses virtual destructors (polymorphic deletion) but also stores `PaintOpType` enum for runtime switching. Rasterizer uses explicit type dispatch:

```cpp
void executeDrawRect(const DrawRectOp& op);
void executeDrawText(const DrawTextOp& op);
```

The inheritance serves only as type marker. A `std::variant` would be more honest about the non-polymorphic dispatch pattern.

---

## Key Remediation Patterns

1. **Extract shared types** to dedicated headers: `caret_state.h`, `text_style.h`

2. **Move style information to layout objects** instead of deriving from semantic types at paint-time

3. **Separate query service from pipeline coordinator** - MarkdownRenderer should delegate queries to a dedicated hit-test/position-mapping service

4. **Complete or remove stub code** - dead methods create architectural weight without value

5. **Unify rect operation types** with role discrimination rather than type proliferation
