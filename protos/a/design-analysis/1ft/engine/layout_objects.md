# Design Analysis: layout_objects.h / layout_objects.cpp

## 1. Responsibilities

The layout_objects module defines the **layout tree** - a parallel structure to the markdown AST that holds computed geometric information (position, size) for rendering.

### Core Duties:
- **Geometry storage**: `Point`, `Size`, `Rect` primitives (lines 8-23 in .h)
- **Flow semantics**: `LayoutFlow::Block` vs `LayoutFlow::Inline` (lines 25-28 in .h)
- **Tree structure**: Parent/child relationships via `addChild()`, `setParent()` (lines 41, 52 in .h)
- **Layout computation**: Each subclass implements `layout(availableSpace)` to compute its rect
- **Text shaping**: `TextLayoutObject` performs glyph shaping and line wrapping (lines 196-369 in .cpp)
- **DOM position tracking**: `getDOMLength()`, `isAtomic()` for cursor/selection support (lines 48-50 in .h)

### Subclass Specializations:
| Class | Flow | Primary Responsibility |
|-------|------|----------------------|
| `BlockLayoutObject` | Block | Vertical stacking of children |
| `InlineLayoutObject` | Inline | Horizontal flow (stub) |
| `TextLayoutObject` | Inline | Glyph shaping, line wrapping, character offsets |
| `ImageLayoutObject` | Block | Image dimension extraction (PNG/JPEG) |
| `TableLayoutObject` | Block | Column width computation, row positioning |
| `TableRowLayoutObject` | Block | Cell positioning within row |
| `TableCellLayoutObject` | Block | Content alignment (left/center/right) |
| `ListItemLayoutObject` | Block | Indent computation, marker handling |

---

## 2. Dependencies

### Header (.h) Dependencies:
```cpp
#include "markdown_objects.h"  // Source AST - owns the content data
#include <ft2build.h>          // FreeType - font loading/metrics
#include FT_FREETYPE_H
#include <vector>
#include <memory>
```

### Implementation (.cpp) Dependencies:
```cpp
#include "typography.h"        // Constants: BASE_FONT_SIZE, LIST_INDENT, etc.
#include "utf8.h"              // UTF-8 decode/length utilities
#include <jpeglib.h>           // JPEG dimension extraction
#include "stb/stb_image.h"     // Image file dimension queries
```

### Dependency Analysis:

**Appropriate Dependencies:**
- `markdown_objects.h`: Layout objects wrap markdown objects - this is the intended data flow
- `typography.h`: Centralized typography constants - good separation
- `utf8.h`: Text processing requires codepoint iteration

**Questionable Dependencies:**
- **FreeType in header** (lines 3-4 in .h): Leaks rendering-layer concern into layout layer. `FT_Face` is exposed in the public API (`setFontFace`), coupling all consumers to FreeType.
- **jpeglib.h, stb_image.h** (lines 4-5 in .cpp): Image dimension extraction belongs in a resource/asset layer, not layout. `ImageLayoutObject` directly decodes base64 and parses image headers (lines 436-525 in .cpp).

---

## 3. Mutation Points

### State Owned by Layout Objects:
| Field | Mutated By | Authority Concern |
|-------|------------|-------------------|
| `rect` | `setRect()`, `layout()` | Layout engine should be sole authority |
| `children` | `addChild()` | Tree builder only |
| `parent` | `setParent()` (via `addChild`) | Tree builder only |
| `fontFace`, `monoFontFace` | `setFontFace()`, `setMonoFontFace()` | Injected from LayoutEngine |
| `glyphRuns`, `charXOffsets`, `lines` | `shapeText()`, `wrapText()` | Internal to TextLayoutObject |
| `isMonospace` | `setMonospace()` | External toggle |
| `intrinsicSize`, `sizeComputed` | `computeImageSize()` | Lazy computation (mutable) |

### Authority Issues:

1. **Font face injection fragility** (lines 107-110, 170-176 in .cpp): `TextLayoutObject` receives font faces via setters after construction. If `layout()` is called before fonts are set, fallback logic kicks in (line 143-145, 260-274 in .cpp), producing inconsistent results.

2. **Rect mutation during layout**: Each layout object's `layout()` method calls `setRect()` on itself (e.g., line 91 in .cpp) AND on its children (e.g., line 87 in .cpp). This dual mutation makes it unclear who "owns" the rect computation.

3. **Lazy image size computation** (lines 141-142 in .h): `mutable` fields `intrinsicSize` and `sizeComputed` in `ImageLayoutObject` allow mutation from `const` methods. This hides I/O (file access, base64 decode) in what appears to be a pure getter.

---

## 4. Boundary Violations

### Layering Concerns:

1. **Image I/O in layout layer** (lines 467-525 in .cpp):
   - `ImageLayoutObject::computeImageSize()` performs:
     - File system access via `stbi_info()` (line 479)
     - Base64 decoding (line 494)
     - PNG header parsing (lines 497-507)
     - JPEG decompression init (lines 512-524)
   - This is resource loading, not layout computation.

2. **FreeType calls in layout** (lines 132-141, 224-227, 230-259 in .cpp):
   - `computeIntrinsicSize()` calls `FT_Set_Pixel_Sizes()`, `FT_Get_Char_Index()`, `FT_Load_Glyph()`
   - `shapeText()` performs full glyph shaping
   - Layout layer is doing text rasterizer's job

3. **Static helper in .cpp** (lines 436-465):
   - `decodeBase64()` is a general utility function embedded in layout code

### Intended vs Actual Layer:
```
Intended:    MarkdownParser -> MarkdownObjects -> LayoutEngine -> LayoutObjects -> Painter
                                                       |
                                                  (positions only)

Actual:      MarkdownParser -> MarkdownObjects -> LayoutEngine -> LayoutObjects -> Painter
                                                       |              |
                                                  (positions)    (glyph shaping,
                                                                  image loading,
                                                                  base64 decode)
```

---

## 5. Declared-but-Unrealised Design

### Incomplete Abstractions:

1. **`InlineLayoutObject::layout()` is a stub** (lines 98-100 in .cpp):
   ```cpp
   void InlineLayoutObject::layout(const Size& availableSpace) {
       // TODO: Implement inline layout (horizontal flow with line breaking)
   }
   ```
   The class exists but does nothing. `TextLayoutObject` handles all inline layout internally via `wrapText()`.

2. **Base `LayoutObject::layout()` is a stub** (lines 44-46 in .cpp):
   ```cpp
   void LayoutObject::layout(const Size& availableSpace) {
       // TODO: Implement base layout
   }
   ```
   Every subclass overrides this, making the base implementation dead code.

3. **Base `LayoutObject::computeIntrinsicSize()` returns zero** (lines 39-42 in .cpp):
   ```cpp
   Size LayoutObject::computeIntrinsicSize() const {
       // TODO: Implement intrinsic size computation
       return Size(0, 0);
   }
   ```
   Only `TextLayoutObject` and `ImageLayoutObject` override this. Other layout objects (tables, lists) don't compute intrinsic sizes, relying on available-space-based layout.

4. **`GlyphRun` struct unused** (lines 81-86 in .h):
   ```cpp
   struct GlyphRun {
       std::vector<uint32_t> glyphIds;
       std::vector<Point> positions;
       float width;
       float height;
   };
   ```
   - `glyphRuns` vector is declared (line 119 in .h) and cleared in `shapeText()` (line 197 in .cpp)
   - `getGlyphRuns()` returns it (lines 166-168 in .cpp)
   - But `shapeText()` never populates it - glyph data goes into `charXOffsets` instead
   - This suggests an intended text-shaping abstraction that was bypassed

5. **`LayoutFlow` enum partially used** (lines 25-28 in .h):
   - All objects declare a flow type
   - But `BlockLayoutObject::layout()` doesn't check children's flow
   - `InlineLayoutObject` is a stub
   - The flow distinction exists in type but not in behavior

### Workaround Code:

1. **Parent-walking for font size** (lines 150-156 in .cpp):
   ```cpp
   float TextLayoutObject::getFontSize() const {
       if (parent) {
           return parent->getFontSize();
       }
       return Typography::BASE_FONT_SIZE;
   }
   ```
   Text objects must walk up to find their font size because it's not passed during construction. This creates implicit coupling to parent structure.

2. **Static empty vectors for missing ranges** (lines 405-406, 414-415 in .cpp):
   ```cpp
   static std::vector<InlineLinkRange> empty;
   return empty;
   ```
   `getLinkRanges()` and `getStyleRanges()` return static empty vectors when parent is null. This is a workaround for nullable parent references.

3. **Dynamic cast in table layout** (lines 558, 589 in .cpp):
   ```cpp
   if (TableRowLayoutObject* rowLayout = dynamic_cast<TableRowLayoutObject*>(child.get()))
   TableLayoutObject* tableLayout = dynamic_cast<TableLayoutObject*>(parent);
   ```
   Parent-child relationships require runtime type checking because the type system doesn't encode the table->row->cell hierarchy.

4. **Empty line special case** (lines 181-183 in .cpp):
   ```cpp
   int len = static_cast<int>(utf8::length(sourceObject->getText()));
   // Empty lines still occupy 1 DOM position (like a newline)
   return (len == 0) ? 1 : len;
   ```
   DOM length computation has special-case logic for empty text, suggesting the DOM position model doesn't cleanly handle whitespace-only content.

---

## Summary of Architectural Concerns

| Issue | Severity | Location |
|-------|----------|----------|
| FreeType in public header | Medium | .h:3-4 |
| Image I/O in layout layer | High | .cpp:467-525 |
| GlyphRun struct unused | Low | .h:81-86 |
| InlineLayoutObject stub | Medium | .cpp:98-100 |
| Font injection fragility | Medium | .cpp:170-176, 230 |
| Dynamic casts for type hierarchy | Low | .cpp:558, 589 |
| Base64 decode in layout | High | .cpp:436-465 |
| Mutable lazy computation hides I/O | Medium | .h:141-142 |
