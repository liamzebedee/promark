# Design Analysis: paint_operations.h / paint_operations.cpp

## Overview

This module defines the **paint operation vocabulary** - the set of drawing primitives that form the intermediate representation between layout and rasterization. It implements a display list pattern where painting is separated from rendering.

**Files:**
- `/home/liam/Documents/projects/promark/protos/a/src/engine/paint_operations.h`
- `/home/liam/Documents/projects/promark/protos/a/src/engine/paint_operations.cpp`

---

## 1. Responsibilities

This module must:

1. **Define the paint operation type hierarchy** - A polymorphic `PaintOp` base class with concrete subclasses for each drawing primitive (lines 26-153 in header)

2. **Provide the DisplayList container type** - A `std::vector<std::unique_ptr<PaintOp>>` typedef (line 155)

3. **Define the Color primitive** - RGBA color representation (lines 8-12)

4. **Enumerate operation types** - `PaintOpType` enum for runtime type discrimination (lines 14-24)

The operation vocabulary includes:
| Operation | Purpose |
|-----------|---------|
| `DrawRectOp` | Filled rectangles (backgrounds) |
| `DrawTextOp` | Styled text runs |
| `DrawImageOp` | Image blitting |
| `SetClipOp` / `RestoreClipOp` | Clip stack management |
| `DrawDebugBorderOp` | Development visualization |
| `DrawCaretOp` | Text cursor |
| `DrawSelectionRectOp` | Selection highlighting |
| `DrawLineOp` | Underlines, blockquote bars |

---

## 2. Dependencies

### Direct Imports

```cpp
#include "layout_objects.h"   // For Rect, Point, Size (line 2)
#include "markdown_objects.h" // For TextStyle enum (line 3)
```

### Dependency Analysis

| Dependency | What's Used | Why |
|------------|-------------|-----|
| `layout_objects.h` | `Rect`, `Point` | Geometric primitives for positioning paint operations |
| `markdown_objects.h` | `TextStyle` enum | Style information (`Bold`, `Italic`, `Code`) for text rendering |

### Dependency Chain

```
markdown_objects.h
        |
        v
layout_objects.h  -->  paint_operations.h
        |
        v
    [FreeType]
```

The import of `markdown_objects.h` (line 3) exists **solely** for the `TextStyle` enum, as noted in the inline comment. This creates a reverse dependency from the paint layer back to the parse layer.

---

## 3. Mutation Points

This module is **entirely immutable after construction**. All paint operations are:

1. Constructed with their full state via constructor parameters
2. Expose only `const` getters
3. Store their data by value (no pointers to mutable state)

**Authority:** The `Painter` class (in `painter.h`) holds exclusive authority over DisplayList creation. The `Rasterizer` consumes DisplayLists read-only.

**State Flow:**
```
Painter::paint() --> creates DisplayList --> Rasterizer::rasterize() consumes
```

The DisplayList itself is a vector of unique_ptrs, meaning ownership transfers when the list is moved. There is no shared state between frames.

---

## 4. Boundary Violations

### Violation #1: Parse Layer Dependency (line 3)

```cpp
#include "markdown_objects.h"  // For TextStyle
```

**Issue:** Paint operations should be a pure rendering abstraction, independent of document structure. The `TextStyle` enum is defined in the parse layer (`markdown_objects.h`) but is used by:
- `DrawTextOp` (line 52, 59, 66-67)
- The downstream `Rasterizer` for font face selection

**Correct Layering:**
```
markdown_objects  -->  layout_objects  -->  paint_operations  -->  rasterizer
     [parse]             [layout]            [paint]              [render]
```

**Actual Layering:**
```
markdown_objects <-------------------------- paint_operations
     [parse]                                     [paint]
                 ^-- INVERSION
```

The `TextStyle` enum should either:
1. Be extracted to a shared `text_types.h` or `font_types.h` header
2. Be redeclared in paint_operations.h (violates DRY but maintains layering)

### Violation #2: Transitive Dependency on FreeType

Via `layout_objects.h` -> FreeType headers. Paint operations shouldn't care about font rasterization technology.

---

## 5. Declared-but-Unrealised Design

### 5.1 Virtual Destructor Pattern (lines 26-35)

```cpp
class PaintOp {
public:
    PaintOp(PaintOpType type);
    virtual ~PaintOp();
    PaintOpType getType() const;
private:
    PaintOpType type;
};
```

**Tension:** The class hierarchy uses virtual destructors (implying polymorphic deletion) but also stores a `PaintOpType` enum that enables runtime type switching. This dual-dispatch pattern suggests uncertainty about the abstraction:

- If operations are truly polymorphic, why maintain a type enum?
- If we need type switching, why use inheritance at all?

**Evidence from Rasterizer (rasterizer.h, lines 28-36):**
```cpp
void executeDrawRect(const DrawRectOp& op);
void executeDrawText(const DrawTextOp& op);
// ... explicit method per type
```

The consumer performs explicit type dispatch rather than using virtual methods, making the inheritance hierarchy serve only as a type marker + container for heterogeneous data.

**Alternative:** A tagged union / `std::variant<DrawRectOp, DrawTextOp, ...>` would be more honest about the non-polymorphic dispatch pattern.

### 5.2 Clip Stack Abstraction (lines 83-96)

```cpp
class SetClipOp : public PaintOp {
    Rect clipRect;
};

class RestoreClipOp : public PaintOp {
    // no data
};
```

**Issue:** This declares a clip stack abstraction (push/pop) but:
1. `RestoreClipOp` has no clip rect - it's a pure sentinel
2. The stack semantics are implicit, not enforced
3. Nested clips aren't represented (no clip intersection)

The Rasterizer must maintain the actual clip stack state externally (`currentClip`, `hasClip` in rasterizer.h lines 47-48). The paint operation layer declares the intent but doesn't model the stack.

### 5.3 DrawDebugBorderOp Asymmetry (lines 98-108)

```cpp
class DrawDebugBorderOp : public PaintOp {
    Rect rect;
    Color color;
};
```

This is structurally identical to `DrawRectOp` but exists as a separate type. The asymmetry suggests:
- Debug borders might need different rendering (e.g., dashed lines)
- Or they should be filterable from the display list
- Or this should be a boolean flag on `DrawRectOp`

Currently, the separation provides type discrimination without behavioral difference.

### 5.4 DrawSelectionRectOp vs DrawRectOp (lines 126-136)

Same structural pattern as above. The comment "painted behind text" (line 125) suggests z-ordering semantics that aren't modeled - the DisplayList is just a flat vector with implicit ordering.

---

## Summary of Architectural Concerns

| Issue | Severity | Recommendation |
|-------|----------|----------------|
| `TextStyle` import from parse layer | Medium | Extract to shared header |
| Inheritance + type enum dual-dispatch | Low | Consider `std::variant` |
| Implicit clip stack semantics | Low | Document contract or model stack |
| Debug/Selection rect type proliferation | Low | Consider flags or unified rect type with role enum |

The module is well-implemented for its current scope but shows signs of organic growth (adding operation types as needed) rather than principled abstraction (defining a minimal orthogonal primitive set).
