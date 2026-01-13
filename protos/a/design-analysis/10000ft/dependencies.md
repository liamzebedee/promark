# Dependency Analysis: Promark Architecture

This document analyzes the dependency structure of the Promark codebase, documenting the intended architecture versus actual implementation, identifying violations, and cataloging technical debt.

---

## 1. Intended Layer Diagram

The architecture suggests a clean downward-only dependency structure:

```
+------------------+
|      SHELL       |  main.cpp, edit.cpp
|  (GLFW, Input)   |  - Window management, input forwarding
+--------+---------+
         |
         v
+------------------+
|      ENGINE      |  engine.cpp
|  (Coordination)  |  - Input handling, state management, render dispatch
+--------+---------+
         |
         v
+------------------+
|  DOCUMENT MODEL  |  markdown_parser, markdown_objects
|     (Parse)      |  - Text -> DOM tree transformation
+--------+---------+
         |
         v
+------------------+
|      LAYOUT      |  layout_engine, layout_objects
|   (Geometry)     |  - DOM -> positioned geometry tree
+--------+---------+
         |
         v
+------------------+
|      PAINT       |  painter, paint_operations
|  (Display List)  |  - Geometry -> drawing commands
+--------+---------+
         |
         v
+------------------+
|   RASTERIZE      |  rasterizer, batch_renderer, glyph_atlas
|     (GPU)        |  - Commands -> pixels
+------------------+
         |
         v
+------------------+
|    UTILITIES     |  text_buffer, clipboard, utf8, typography
|   (Foundation)   |  - Shared primitives, no domain knowledge
+------------------+
```

**Expected rules:**
- Each layer imports only from layers below
- Platform dependencies (GLFW, OpenGL, FreeType) are isolated at boundaries
- Shared types live in utilities, not scattered across layers

---

## 2. Actual Dependencies: The Real Graph

### 2.1 Shell Layer Violations

**Shell reaches into Engine internals:**
- `edit.cpp` bypasses `Engine::isDirty()` and `Engine::markClean()`, implementing parallel dirty tracking by comparing `engine->getContent()` against cached string
- Both shells call `engine->insertText()` directly after transforming dropped files, bypassing expected `handleFileDrop()` input API
- Shell generates markdown syntax (`![alt](data:...)`), pushing domain knowledge outward

**Shell loses information Engine needs:**
- Coordinate scaling computed at shell, then discarded
- Engine receives scaled coordinates with no access to scale factor or physical pixels

### 2.2 Engine Layer Violations

**Engine bypasses its own abstractions:**
```
Engine
  |-- owns TextBuffer (declared editing API)
  |-- owns inputBuffer[10MB] (actual authority)
  |-- dual-writes both on every mutation
```

The `TextBuffer::insertText()`/`deleteText()` API is never called. Engine performs raw `memmove`/`memcpy` operations, then copies the entire buffer to TextBuffer via `setText()`.

**Engine embeds platform dependencies:**
- GLFW key constants (`GLFW_KEY_*`, `GLFW_MOD_*`) in 260+ lines
- `glfwGetTime()` calls in 8 locations
- OpenGL calls in `initialize()` and `render()`

**Engine reimplements lower-layer logic:**
- Raw mode (230 lines) duplicates MarkdownRenderer's text wrapping, hit testing, cursor positioning, selection rendering
- Position translation (`rawToDOM`/`domToRaw`) duplicates parser's position tracking

### 2.3 Document Model Violations

**Parser has deep knowledge of all object types:**
- No factory abstraction; parser calls constructors directly
- Adding a new markdown element requires parser modification

**Three competing inline formatting models:**
1. Tree nodes (`MarkdownObjectType::Bold/Italic/Underline`) - never instantiated
2. Span annotations (`InlineStyleRange`) - actual runtime model
3. Bitmask flags (`TextStyle` enum) - used by rendering

### 2.4 Layout Layer Violations

**FreeType types in public API:**
```cpp
void setFontFace(FT_Face face);
FT_Face getFontFace() const;
```

Both LayoutEngine and TextLayoutObject expose `FT_Face` in their interfaces. Consumers are coupled to FreeType.

**I/O embedded in layout:**
- ImageLayoutObject performs file system access (`stbi_info`)
- Base64 decoding, PNG header parsing, JPEG decompression in layout layer
- Asset loading disguised as geometry computation

**Split layout authority:**
| Component | Responsibility |
|-----------|---------------|
| LayoutEngine | Block children positioning |
| TextLayoutObject | Text shaping, line wrapping |
| TableLayoutObject | Table cell layout |
| ListItemLayoutObject | Indent + marker positioning |

Four of eight layout types fully self-manage; the engine coordinate role is inconsistent.

### 2.5 Rendering Layer Violations

**Paint imports from parse layer:**
```cpp
#include "markdown_objects.h"  // For TextStyle enum only
```

Painter queries `MarkdownObjectType` to make rendering decisions:
```cpp
if (layoutObject->getSourceObject()->getType() == MarkdownObjectType::BlockQuote)
```

Style should flow from layout objects, not be reconstructed from document semantics.

**Circular awareness:**
| Module | Imports | Why |
|--------|---------|-----|
| paint_operations.h | markdown_objects.h | TextStyle enum |
| painter.cpp | markdown_renderer.h | CaretState struct |
| markdown_renderer.h | ft2build.h | FT_Face in API |

### 2.6 Rasterization Layer Violations

**Split GL state authority:**
| Component | GL State Modified |
|-----------|-------------------|
| Rasterizer | Scissor test, texture creation/binding |
| BatchRenderer | Blend mode, program, vertex attribs |
| GlyphAtlas | GL_UNPACK_ALIGNMENT, texture binding |

BatchRenderer was designed as sole GL abstraction, but Rasterizer makes direct calls.

**FreeType leakage through all levels:**
```
Rasterizer.h    -> FT_Library, FT_Face members
BatchRenderer.h -> FT_Face in drawText signature
GlyphAtlas.h    -> FT_Face parameter to get()
```

GlyphAtlas modifies foreign state: `FT_Set_Pixel_Sizes(face, ...)` on faces it doesn't own.

**Text shaping in wrong layer:**
- UTF-8 decoding in BatchRenderer (should be text layer)
- Glyph positioning (`penX += advance`) in BatchRenderer (should be layout)

---

## 3. Dependency Inversions

### 3.1 Low-Level Depends on High-Level

| Low-Level Module | Depends On | Type Needed |
|------------------|------------|-------------|
| paint_operations.h | markdown_objects.h | `TextStyle` enum (4 values) |
| glyph_atlas.h | markdown_objects.h | `TextStyle` enum |
| batch_renderer.h | markdown_objects.h | `TextStyle` enum |
| painter.cpp | markdown_renderer.h | `CaretState` struct |

**Root cause:** Shared types defined where first needed, not in dedicated headers.

### 3.2 Abstraction Layer Bypassed

| Abstraction | Declared In | Bypassed By |
|-------------|-------------|-------------|
| TextBuffer editing API | text_buffer.h | Engine (raw buffer manipulation) |
| Engine dirty/close API | engine.h | Shell (parallel implementation) |
| LayoutObject::layout() | layout_objects.h | LayoutEngine (type-switch dispatch) |
| PaintOp display list | paint_operations.h | Rasterizer (manual dispatch) |

### 3.3 Platform Details Leak Upward

**FreeType exposure chain:**
```
GlyphAtlas.get(FT_Face, ...)
    ^
    |
BatchRenderer.drawText(FT_Face, ...)
    ^
    |
Rasterizer.executeDrawText() [owns FT_Face, passes down]
    ^
    |
MarkdownRenderer [exposes FT_Face in public API]
    ^
    |
LayoutEngine.setFontFace(FT_Face)
```

FT_Face threads through 5 layers. No abstraction hides this detail.

**GLFW exposure:**
- Engine uses GLFW key constants directly
- Clipboard hard-includes GLFW despite "platform-agnostic" claim
- Shell-level coordinate scaling cannot be accessed by Engine

---

## 4. Circular Dependencies

### 4.1 Engine <-> Shell

**Forward:** Shell creates and owns Engine, calls input handlers.
**Backward:** Shell polls Engine state every frame (`isDirty`, `getContent`, `shouldClose`).
**Workaround:** Shell maintains parallel dirty state, ignoring Engine's tracking.

### 4.2 MarkdownRenderer <-> Painter

**Forward:** MarkdownRenderer creates and invokes Painter.
**Backward:** Painter imports CaretState from markdown_renderer.h.
**Workaround:** CaretState passed as parameter, but compile-time coupling remains.

### 4.3 Layout <-> Document Model

**Forward:** Layout consumes MarkdownObject trees.
**Backward:** LayoutObjects store `const MarkdownObject*` pointers.
**Tension:** Layout queries document semantics at paint time rather than caching visual properties.

### 4.4 Text Authority Triangle

```
Engine::inputBuffer[10MB]  <--authoritative
        |
        | copies to
        v
Engine::textBuffer (TextBuffer)
        |
        | copies to
        v
MarkdownRenderer::textBuffer (unique_ptr<TextBuffer>)
```

Each mutation requires three-way synchronization. The `unique_ptr` ownership pattern implies transfer but actual behavior is value copy.

---

## 5. Transitive Dependencies

### 5.1 FreeType Leakage Path

Any module importing LayoutEngine transitively depends on FreeType:
```
client code
    |
    v
layout_engine.h
    |
    includes
    v
<ft2build.h>
```

**Impact:** Unit testing layout requires FreeType linkage.

### 5.2 GLFW Leakage Paths

**Via Clipboard:**
```
any module using clipboard
    |
    v
clipboard.h
    |
    includes
    v
<GLFW/glfw3.h>
```

**Via Engine:**
Engine cannot be compiled without GLFW key constant definitions.

### 5.3 OpenGL Leakage Paths

**Via gl_includes.h:**
```
BatchRenderer, GlyphAtlas, Rasterizer
    |
    include
    v
gl_includes.h
    |
    includes
    v
<GL/gl.h> or <OpenGL/gl.h>
```

**Direct calls:**
Engine makes direct OpenGL calls in `initialize()` and `render()`, bypassing batch renderer.

### 5.4 markdown_objects.h Spread

The document model header is imported by:
- markdown_parser.h (expected)
- layout_engine.h (expected)
- paint_operations.h (violation - for TextStyle)
- glyph_atlas.h (violation - for TextStyle)
- batch_renderer.h (violation - for TextStyle)

Changes to markdown object definitions force recompilation of entire rendering stack.

---

## 6. Clean Layering: What It Should Look Like

### 6.1 Ideal Dependency Structure

```
Shell
  |
  | (input events, lifecycle signals)
  v
Engine
  |
  | (text operations, render requests)
  v
MarkdownRenderer [facade]
  |
  +-- MarkdownParser -> MarkdownObjects
  |
  +-- LayoutEngine -> LayoutObjects
  |         |
  |         | (Font abstraction, not FT_Face)
  |         v
  |     FontProvider [interface]
  |
  +-- Painter -> PaintOperations
  |
  +-- Rasterizer
          |
          | (GL abstraction)
          v
      RenderBackend [interface]
```

### 6.2 Type Extraction Required

**New shared headers:**
| Header | Contents | Currently In |
|--------|----------|--------------|
| text_style.h | `enum TextStyle { Normal, Bold, Italic, Code }` | markdown_objects.h |
| caret_state.h | `struct CaretState { ... }` | markdown_renderer.h |
| font_types.h | Font abstraction (not FT_Face) | scattered |
| geometry.h | Point, Size, Rect | layout_objects.h |

### 6.3 Platform Isolation

**Font abstraction:**
```cpp
// Instead of:
void setFontFace(FT_Face face);

// Should be:
void setFont(const Font& font);  // Font wraps FT_Face internally
```

**Input abstraction:**
```cpp
// Instead of:
if (key == GLFW_KEY_BACKSPACE && mods & GLFW_MOD_CONTROL)

// Should be:
if (input.isWordDelete())  // Shell translates platform keys
```

**GL abstraction:**
```cpp
// BatchRenderer should be sole GL caller
// Rasterizer should not call glScissor, glGenTextures directly
```

### 6.4 Authority Consolidation

**Single text authority:**
```cpp
// TextBuffer should own the string, provide editing API
// Engine should use that API, not maintain parallel char[]
// MarkdownRenderer should receive const reference, not ownership copy
```

**Single dirty tracking:**
```cpp
// Engine owns dirty state
// Shell queries Engine, never maintains parallel tracking
```

---

## 7. Violation Summary Matrix

| Layer | Imports From Above | Imports Platform | Bypasses Own Abstraction |
|-------|-------------------|------------------|-------------------------|
| Shell | - | GLFW | Ignores Engine::isDirty |
| Engine | - | GLFW, OpenGL | Bypasses TextBuffer API |
| Document | - | - | Three inline format models |
| Layout | - | FreeType | I/O in layout |
| Paint | Document (TextStyle) | - | Queries semantic types |
| Rasterize | Document (TextStyle) | OpenGL, FreeType | Direct GL alongside BatchRenderer |
| Utilities | - | GLFW (Clipboard) | - |

---

## 8. Remediation Priority

### High: Breaking Changes Required

1. **Extract TextStyle enum** to shared header - breaks compilation of 5+ files but removes 3 dependency inversions
2. **Abstract FT_Face** behind Font interface - large refactor but enables testing and portability
3. **Consolidate text authority** - Engine should use TextBuffer API or own the buffer, not both

### Medium: Incremental Cleanup

4. **Move CaretState** to shared header
5. **Remove parallel dirty tracking** from Shell
6. **Extract image loading** from layout layer to resource service
7. **Consolidate GL calls** in BatchRenderer

### Low: Technical Debt Reduction

8. **Remove dead code** - stubbed detection methods, unused PaintOp types
9. **Pick one inline formatting model**
10. **Add platform abstraction** for input constants

---

## 9. Dependency Metrics

| Metric | Count |
|--------|-------|
| Direct dependency inversions | 4 |
| Circular dependency pairs | 4 |
| Transitive platform leakages | 3 (FreeType, GLFW, OpenGL) |
| Bypassed abstractions | 4 (TextBuffer, dirty tracking, layout(), PaintOp) |
| Modules importing markdown_objects.h | 8 (5 inappropriate) |
| FT_Face appearances in public APIs | 5 layers |

The architecture exhibits a pattern of **structural abstraction without mechanical enforcement** - interfaces exist but are routinely bypassed, types are declared but not used, and platform details propagate through layers that should be platform-agnostic.
