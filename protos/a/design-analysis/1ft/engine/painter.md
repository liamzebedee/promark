# Painter Module Design Analysis

**Files:** `src/engine/painter.h`, `src/engine/painter.cpp`

---

## 1. Responsibilities

The Painter module converts a layout tree into a display list (sequence of draw operations). Its core responsibilities:

1. **Tree traversal** - Walk the LayoutObject hierarchy depth-first, painting backgrounds before content, content before borders (lines 36-68 in .cpp)

2. **Type dispatch** - Route each LayoutObject to its appropriate paint method via dynamic_cast chain (lines 44-56)

3. **Text rendering** - Convert TextLayoutObject content into DrawTextOp sequences with style/link segment handling (lines 71-173)

4. **Caret positioning** - Translate global DOM cursor positions into screen coordinates for caret rendering (lines 362-419)

5. **Selection geometry** - Compute per-line selection rectangles for multi-line wrapped text (lines 421-488)

6. **Display list production** - Output a flat vector of PaintOp objects consumable by the Rasterizer

---

## 2. Dependencies

### Direct Includes (painter.cpp)

| Include | Purpose | Concern |
|---------|---------|---------|
| `paint_operations.h` | DisplayList, all *Op types | Core output abstraction |
| `layout_objects.h` | LayoutObject hierarchy | Core input abstraction |
| `markdown_renderer.h` | CaretState struct | **Upward dependency** (see section 4) |
| `typography.h` | BASE_FONT_SIZE, LIST_INDENT constants | Layout policy leaking into paint |
| `utf8.h` | String iteration for text segments | Utility |

### Implicit Dependencies

- **MarkdownObject types** - The painter queries `getSourceObject()->getType()` to make paint decisions (lines 40, 314-321, 324-335, 590). This couples paint to markdown semantics.

- **FreeType** - Transitively via layout_objects.h (FT_Face types). Not used directly in painter.

---

## 3. Mutation Points

The Painter is stateless between calls. It holds no mutable fields.

| Mutation | Location | Authority |
|----------|----------|-----------|
| DisplayList accumulation | `displayList.push_back()` throughout | Painter (correct) |
| Environment read | `std::getenv("DEBUG")` at line 61 | External process env |

**No layout tree mutation** - Painter correctly takes `const LayoutObject*` and never modifies it.

**Authority concern**: Debug border painting is controlled by environment variable rather than explicit API parameter. This makes paint output non-deterministic from API perspective.

---

## 4. Boundary Violations

### 4.1 Upward Dependency on MarkdownRenderer

**Location:** painter.h line 8, painter.cpp line 2

```cpp
// Forward declaration - defined in markdown_renderer.h
struct CaretState;
```

```cpp
#include "markdown_renderer.h"  // For CaretState
```

The Painter lives below MarkdownRenderer in the architecture (MarkdownRenderer owns Painter at line 66 of markdown_renderer.h). Yet Painter imports CaretState from markdown_renderer.h, creating a circular dependency graph:

```
MarkdownRenderer -> Painter -> markdown_renderer.h
```

**Correct resolution:** CaretState should be defined in a separate header (e.g., `caret_state.h` or `selection.h`) that both modules can import.

### 4.2 Layout Policy in Paint Layer

**Location:** painter.cpp lines 269-271, 278, 290

```cpp
float textIndent = Typography::LIST_INDENT * (indentLevel + 1);
float markerX = rect.position.x + textIndent - Typography::LIST_INDENT + 2.0f;
float markerY = textY + Typography::BASE_FONT_SIZE;  // Baseline aligned with text
```

The Painter computes layout offsets using Typography constants. This violates the separation between layout (spatial positioning) and paint (visual output). The ListItemLayoutObject should provide `getMarkerPosition()` or the marker should be a separate child layout object.

### 4.3 Semantic Queries During Paint

**Location:** painter.cpp lines 40-41, 313-335, 586-595

The painter queries markdown object types to make paint decisions:

```cpp
if (layoutObject->getSourceObject()->getType() == MarkdownObjectType::BlockQuote) {
    paintBlockQuoteBar(layoutObject, displayList);
}
```

```cpp
Color Painter::getTextColor(const MarkdownObject* object) {
    switch (object->getType()) {
        case MarkdownObjectType::Heading:
            return Color(0, 0, 0, 255);
        case MarkdownObjectType::Link:
            return Color(0, 0, 255, 255);
```

Paint should respond to visual properties (colors, borders, decorations) declared on layout objects, not derive them from semantic type. The LayoutObject should carry style information.

---

## 5. Declared-but-Unrealised Design

### 5.1 Unused paintBorder Method

**Declaration:** painter.h line 30
**Implementation:** painter.cpp lines 307-311

```cpp
void Painter::paintBorder(const LayoutObject* layoutObject, DisplayList& displayList) {
    (void)layoutObject;
    (void)displayList;
    // Border painting not yet implemented
}
```

Border painting is invoked for every layout object (line 58) but does nothing. The signature promises per-element border support; the stub means actual borders are hand-coded elsewhere (e.g., table borders in paintTable lines 189-227, blockquote bar in paintBlockQuoteBar).

### 5.2 paintLinkUnderline Never Called

**Declaration:** painter.h line 33
**Implementation:** painter.cpp lines 553-584

The method `paintLinkUnderline` is fully implemented but never invoked. Link underlines are instead painted inline within `paintText` (lines 158-168). This creates:

- Dead code (the standalone method)
- Duplicated logic (underline drawing in two places)
- Inconsistent abstraction (other paint* methods are called from paintLayoutObject)

### 5.3 isInsideLink Never Called

**Declaration:** painter.h line 34
**Implementation:** painter.cpp lines 586-595

```cpp
bool Painter::isInsideLink(const LayoutObject* layoutObject) {
    // Check if this layout object or any ancestor is a link
    const LayoutObject* current = layoutObject;
    while (current) {
        if (current->getSourceObject()->getType() == MarkdownObjectType::Link) {
            return true;
        }
        current = current->getParent();
    }
    return false;
}
```

This ancestor-walking method exists but is never called. Link detection is instead done via `getLinkRanges()` character ranges (line 78, 105-110). The method implies a tree-based link detection strategy that was abandoned for range-based approach.

### 5.4 SetClipOp/RestoreClipOp Never Emitted

**Declared:** paint_operations.h lines 83-96

The paint operations system defines clipping operations, but the Painter never emits them. This implies intended support for:
- Overflow clipping
- Viewport culling
- Nested scroll containers

None of these are implemented. All content is painted regardless of visibility.

### 5.5 Asymmetric Table Cell Painting

**Location:** painter.cpp lines 248-253

```cpp
void Painter::paintTableCell(const TableCellLayoutObject* cellObject, DisplayList& displayList) {
    // Cell content is painted via children (text nodes)
    // This function could be used for cell-specific styling if needed
    (void)cellObject;
    (void)displayList;
}
```

Table and TableRow have painting logic, but TableCell is a stub. The comment admits it exists for "cell-specific styling if needed" - future capability that creates architectural weight without value.

### 5.6 computeXForOffset - Incorrect Monospace Assumption

**Location:** painter.cpp lines 344-360

```cpp
float Painter::computeXForOffset(int offset, const char* text, float fontSize, float startX) {
    // Simple monospace approximation: each char is ~0.6 * fontSize wide
    float charWidth = fontSize * 0.6f;
```

This method uses a hardcoded monospace approximation despite the codebase having proper glyph metrics via FreeType and `TextLayoutObject::getCharXOffset()`. The method appears to be legacy code - it's only called from nowhere observable in the caret/selection code which uses the layout object's character offset methods instead.

### 5.7 DOMPositionResult Underutilized

**Declaration:** painter.h lines 11-15

```cpp
struct DOMPositionResult {
    const LayoutObject* layout = nullptr;
    int localOffset = 0;
    bool isAtomicBoundary = false;  // True if cursor is at edge of atomic element
};
```

The `isAtomicBoundary` field is set during lookup (line 519) but never read by any consumer. It implies cursor behavior differentiation at atomic boundaries (images) that isn't implemented.

---

## Summary of Architectural Issues

| Issue | Severity | Remediation |
|-------|----------|-------------|
| CaretState defined in downstream module | High | Extract to shared header |
| Typography constants used for layout math | Medium | Move marker positioning to layout |
| Semantic type queries during paint | Medium | Add style properties to LayoutObject |
| Dead code: paintLinkUnderline, isInsideLink | Low | Remove or integrate |
| Stub methods: paintBorder, paintTableCell | Low | Remove or implement |
| Environment-based debug toggle | Low | Add explicit debug parameter |
| Unused clipping infrastructure | Low | Document as future or remove |
