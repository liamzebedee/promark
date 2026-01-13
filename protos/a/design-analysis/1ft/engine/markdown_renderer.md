# MarkdownRenderer Design Analysis

## Overview

`MarkdownRenderer` is intended to be a facade coordinating a four-stage rendering pipeline: Parse -> Layout -> Paint -> Rasterize. In practice, it has accumulated significant query logic that bypasses this pipeline and reaches directly into the intermediate trees.

**Files analyzed:**
- `/home/liam/Documents/projects/promark/protos/a/src/engine/markdown_renderer.h`
- `/home/liam/Documents/projects/promark/protos/a/src/engine/markdown_renderer.cpp`

---

## 1. Responsibilities

### Stated Responsibilities (Pipeline Coordination)
- **Buffer management**: Own the `TextBuffer` and trigger re-parse on change (lines 14-19, cpp)
- **Pipeline orchestration**: Drive parse/layout/paint/rasterize in sequence with dirty-flag optimization (lines 45-65, cpp)
- **Font configuration**: Forward font faces to `LayoutEngine` (lines 33-43, cpp)
- **Caret state injection**: Accept external caret state and forward to painter (lines 21-31, cpp)

### Actual Responsibilities (Accumulated Query Logic)
- **DOM<->Raw position mapping**: `domToRaw()`, `rawToDOM()`, `getTotalDOMLength()` (lines 139-181, 507-552, cpp)
- **Hit testing**: Convert screen coordinates to cursor position (lines 201-362, cpp)
- **Cursor position queries**: `getCursorY()`, `getCursorXY()` (lines 364-456, cpp)
- **Link detection**: `getLinkAtPosition()` (lines 458-505, cpp)

The query logic (350+ lines) far exceeds the pipeline coordination logic (~100 lines).

---

## 2. Dependencies

### Direct Includes (markdown_renderer.h, lines 2-8)
| Dependency | Purpose | Coupling Level |
|------------|---------|----------------|
| `text_buffer.h` | Source document storage | Owns unique_ptr |
| `markdown_parser.h` | Parse stage | Owns unique_ptr |
| `layout_engine.h` | Layout stage | Owns unique_ptr |
| `painter.h` | Paint stage | Owns unique_ptr |
| `rasterizer.h` | Rasterize stage | Owns unique_ptr |
| `<ft2build.h>` / `FT_FREETYPE_H` | Font face types | Pass-through only |

### Implicit Dependencies (via dynamic_cast and method calls)
- `TextLayoutObject` (lines 190, 204, etc.): Cast from `LayoutObject*` to access text-specific methods
- `MarkdownObject` tree traversal: Used in `collectTextObjects()` helper (lines 129-137)

### Dependency Concerns

**FreeType header leak**: The renderer includes FreeType headers (lines 7-8) solely to expose `setFontFace(FT_Face)`. This is a low-level type that should be abstracted. The renderer should not know about FreeType; it should accept a font identifier and let `LayoutEngine` resolve it.

**Circular awareness**: `painter.h` forward-declares `CaretState` (defined in `markdown_renderer.h`). This creates a header dependency from a lower layer back to a higher layer.

---

## 3. Mutation Points

### State Owned by MarkdownRenderer
| Field | Set By | Read By | Concern |
|-------|--------|---------|---------|
| `textBuffer` (line 63, h) | `setTextBuffer()` | `parseMarkdown()`, `paint()` | Exclusive ownership, correct |
| `objectTree` (line 69, h) | `parseMarkdown()` | Query functions | Shared read, but not exposed as const& |
| `layoutTree` (line 70, h) | `performLayout()` | `paint()`, all query functions | Heavy read traffic |
| `displayList` (line 71, h) | `paint()` | `rasterize()`, `getDisplayList()` | Appropriate |
| `caretState` (line 73, h) | `setCaretState()` | `paint()`, `rasterize()` | Passed to painter, also used in rasterize |
| `needsReparse/Relayout/Repaint` (lines 75-77, h) | Multiple setters | `render()` | Dirty flags are internal bookkeeping |
| `lastViewportSize` (line 79, h) | `render()` | `render()` | Cache for relayout decision |

### Authority Concerns

**Caret visibility split authority**: `caretState.caretVisible` is passed to `rasterizer.rasterize()` (line 106, cpp), but caret painting happens in `Painter::paintCaret()`. The visibility flag affects both paint-time (what to include) and raster-time (whether to draw). This split creates a subtle contract: painter must emit caret ops regardless of visibility, and rasterizer decides whether to honor them.

**Font face forwarding**: Renderer receives `FT_Face` and forwards to `LayoutEngine` (lines 33-43, cpp). This makes renderer responsible for font lifecycle it doesn't control. If the face is freed externally, layout will crash.

---

## 4. Boundary Violations

### Query Logic Reaches Through Abstraction Layers

The hit-testing and position-mapping code directly traverses both the `MarkdownObject` tree and `LayoutObject` tree, bypassing the pipeline stages that are supposed to be the sole consumers of these structures.

**Example: `hitTest()` (lines 201-362)**
- Calls `collectTextLayoutsWithPos()` which traverses `layoutTree`
- Uses `dynamic_cast<const TextLayoutObject*>` to access line/glyph data
- Accesses `TextLayoutObject::getLines()`, `getCharXOffsetInLine()`, `getFontSize()`
- Duplicates line-Y calculation logic from layout/paint stages

**Example: `domToRaw()` / `rawToDOM()` (lines 154-181, 507-552)**
- Directly traverses `objectTree` via `collectTextObjects()`
- Accesses `MarkdownObject::getRawStart()`, `getRawEnd()`, `getText()`
- This is semantic query logic that arguably belongs on `MarkdownObject` or a dedicated position-mapping service

### Inverted Dependency: CaretState

`CaretState` is defined in `markdown_renderer.h` (lines 12-21) but used by:
- `Painter::paint()` accepts `const CaretState*`
- `Rasterizer::rasterize()` accepts `caretVisible` bool

A lower layer (`painter.h`) forward-declares a type from a higher layer. The struct should live in a shared types header or in `painter.h` itself.

---

## 5. Declared-but-Unrealised Design

### Dirty Flag Optimization (Partially Realized)

The three dirty flags (`needsReparse`, `needsRelayout`, `needsRepaint`) suggest a design where stages can be skipped independently. However:

**Realized**: Reparse triggers relayout triggers repaint (correctly cascades).

**Unrealized**: `rasterize()` is called unconditionally every frame (line 64, cpp). There's no `needsRasterize` flag. The comment "Manual pipeline control for debugging/testing" (line 34, h) suggests the stage methods were meant to be independently callable, but `rasterize()` lacks any dirty-flag optimization.

### Viewport-Dependent Relayout (Asymmetric)

```cpp
// lines 50-54, cpp
if (viewportSize.width != lastViewportSize.width ||
    viewportSize.height != lastViewportSize.height) {
    needsRelayout = true;
    lastViewportSize = viewportSize;
}
```

This checks both width and height, but markdown layout typically only depends on width (vertical scrolling). Height changes shouldn't trigger relayout. This suggests incomplete understanding of which dimension matters, or over-conservative invalidation.

### Animated Caret Position (Declared, Bypassed)

`CaretState` includes animated position fields (lines 18-20, h):
```cpp
float animatedCaretX = 0;
float animatedCaretY = 0;
bool useAnimatedPosition = false;
```

However, `MarkdownRenderer` never reads or writes these fields. They're set externally and presumably consumed by painter/rasterizer. The renderer provides `getCursorXY()` (lines 407-456) to compute the *logical* position, and the caller is expected to animate toward it. This split responsibility is undocumented.

### Manual Pipeline Control (Documented but Awkward)

```cpp
// lines 34-38, h
// Manual pipeline control for debugging/testing
void parseMarkdown();
void performLayout(const Size& availableSpace);
void paint();
void rasterize(const Size& viewportSize, float scrollOffsetY = 0.0f);
```

These are public, but:
- `parseMarkdown()` requires `textBuffer` to be set first
- `performLayout()` requires `objectTree` from parsing
- `paint()` requires `layoutTree` from layout
- `rasterize()` requires `displayList` from paint

There's no API contract enforcing this ordering. Calling `paint()` without prior `performLayout()` silently does nothing (early return at line 94). This is defensive but hides bugs.

### Position Mapping Design Mismatch

The existence of `domToRaw()` and `rawToDOM()` implies a dual-coordinate system:
- **Raw**: Positions in the original markdown text (includes syntax like `**`, `#`)
- **DOM**: Positions in the rendered text (excludes syntax)

However:
- `getTotalDOMLength()` is implemented in renderer (lines 139-152), but should be a property of the object tree
- The mapping logic duplicates tree traversal that `MarkdownParser` already performed
- `MarkdownObject::getRawStart()` / `getRawEnd()` exist, suggesting the parser intended to provide this mapping

**Workaround code**: The `collectTextObjects()` helper (lines 129-137) and `collectTextLayoutsWithPos()` helper (lines 185-199) are traversal utilities that compensate for the lack of a proper iterator or visitor pattern on the tree structures.

### Link Range Queries

`getLinkAtPosition()` (lines 458-505) queries `TextLayoutObject::getLinkRanges()`, but the logic for building link ranges lives in layout. The renderer is doing semantic queries (what link is here?) by reaching into layout data structures. This could be a method on `LayoutObject` or a dedicated hit-test service.

---

## Summary of Architectural Issues

1. **MarkdownRenderer has two distinct roles**: pipeline coordinator and query service. These should be separated.

2. **Query logic duplicates traversal**: Multiple helper functions traverse trees in similar ways. A visitor or cursor abstraction would consolidate this.

3. **CaretState ownership is inverted**: Should be defined in a shared header or in painter, not in renderer.

4. **FreeType types leak through the API**: Renderer should accept abstract font handles, not `FT_Face`.

5. **Dirty flags are incomplete**: `rasterize()` always runs; animated caret fields are unused internally.

6. **Position mapping is scattered**: DOM/Raw conversion logic should live on the object tree, not be reimplemented in renderer queries.

7. **No invalidation for query results**: If `getCursorXY()` is called after text changes but before `render()`, it returns stale data. The API doesn't make this contract clear.
