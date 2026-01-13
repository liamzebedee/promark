# BatchRenderer Design Analysis

**Files**: `src/engine/batch_renderer.h`, `src/engine/batch_renderer.cpp`

---

## 1. Responsibilities

The BatchRenderer module is responsible for:

1. **GPU Resource Management** - Owns and manages OpenGL shader programs (`textProg`, `solidProg`, `imageProg`) and a vertex buffer object (`vbo`) (lines 44-47 .h, 14-18 .cpp)

2. **Batch Accumulation** - Collects draw calls into a vertex buffer to minimize GPU state changes (lines 49-50 .h, `vertices` vector with 60000 max capacity)

3. **Primitive Rendering** - Provides three drawing primitives:
   - `drawQuad`: Textured quads with UV coordinates (lines 26-28 .h, 197-219 .cpp)
   - `drawRect`: Solid color rectangles (lines 29-30 .h, 221-241 .cpp)
   - `drawImage`: External texture rendering (lines 31 .h, 243-291 .cpp)

4. **Text Rendering** - Converts text strings to textured quads via glyph atlas lookups (lines 34-36 .h, 293-320 .cpp)

5. **Projection Management** - Maintains orthographic projection matrix with scroll offset support (lines 52-53 .h, 115-135 .cpp)

---

## 2. Dependencies

### Direct Dependencies

| Dependency | Location | Purpose |
|------------|----------|---------|
| `GlyphAtlas` | line 2 .h, line 55 .h | Text glyph texture atlas; provides UV coordinates and metrics |
| `paint_operations.h` | line 3 .h | Only imports `TextStyle` enum (transitive via markdown_objects.h) |
| `shaders_embedded.h` | line 2 .cpp | Embedded GLSL shader source strings |
| `utf8.h` | line 3 .cpp | UTF-8 decoding for text rendering |
| `gl_includes.h` | line 4 .cpp | OpenGL function declarations |
| `FT_Face` (FreeType) | line 36 .h | Font face handle passed through to atlas |

### Dependency Analysis

**GlyphAtlas coupling (line 55 .h, 159 .cpp, 296-306 .cpp)**:
The renderer holds a raw pointer to `GlyphAtlas` and calls `bind()` and `get()` directly. This creates tight coupling where:
- The renderer must know the atlas texture binding semantics
- The `drawText` method queries glyphs one-at-a-time in a loop (lines 302-318 .cpp)

**FreeType leakage (line 36 .h)**:
`FT_Face` appears in the public interface, exposing a FreeType implementation detail. The BatchRenderer doesn't use `FT_Face` itself - it merely forwards it to `GlyphAtlas::get()`.

---

## 3. Mutation Points

### Internal State

| State | Mutated By | Authority Issue |
|-------|------------|-----------------|
| `vertices` vector | `begin()`, `flush()`, `drawQuad()`, `drawRect()` | Correct - internal batch state |
| `textured` flag | `drawQuad()`, `drawRect()`, `begin()` | Correct - tracks current batch mode |
| `projMatrix[16]` | `setViewport()` | Correct - owned projection state |
| `viewportW/H` | `setViewport()` | Stores but doesn't use for anything except scroll math |

### External State Mutation

| OpenGL State | Where Modified | Issue |
|--------------|----------------|-------|
| Blend mode | lines 172-173, 186, 279-280, 290 .cpp | Enables then disables; assumes caller wants blend off |
| Active texture unit | lines 158, 254 .cpp | Sets to `GL_TEXTURE0` |
| Bound texture | lines 159, 182, 255, 288 .cpp | Binds then unbinds to 0 |
| Current program | lines 149, 183, 248, 289 .cpp | Uses then resets to 0 |
| Vertex attribs 0-2 | lines 163-170, 177-179, 270-286 .cpp | Enables then disables |

**Authority Problem**: The renderer assumes it owns OpenGL blend state, resetting it after each `flush()` and `drawImage()`. This "restore state for immediate mode compatibility" comment (line 185 .cpp) suggests workaround code for an unstated constraint.

---

## 4. Boundary Violations

### Layering Inversions

1. **FreeType in API (line 36 .h)**
   `drawText(..., FT_Face face)` exposes a font-system implementation detail through the rendering interface. The BatchRenderer should not need to know about FreeType - it only needs glyph metrics and UVs.

2. **Text shaping in renderer (lines 298-319 .cpp)**
   The `drawText` method performs text layout:
   ```cpp
   float penX = x;
   while (pos < text.length()) {
       uint32_t codepoint = utf8::decode(text, pos);
       ...
       penX += glyph->advance;
   }
   ```
   This is horizontal text layout logic. A batch renderer should receive positioned glyphs, not shape text itself.

3. **UTF-8 decoding (line 3, 303 .cpp)**
   Character encoding is a text-processing concern, not a rendering concern.

4. **paint_operations.h import (line 3 .h)**
   The header imports paint_operations.h but the BatchRenderer API doesn't use any `PaintOp` types. The import exists solely to get `TextStyle` transitively through `markdown_objects.h`. This creates an unnecessary coupling to the paint operations abstraction.

---

## 5. Declared-but-Unrealised Design

### The `PaintOp` Abstraction Gap

**Declared (paint_operations.h lines 14-24)**: A comprehensive paint operation type system with `PaintOpType` enum including:
- `DrawRect`, `DrawText`, `DrawImage`
- `SetClip`, `RestoreClip`
- `DrawDebugBorder`, `DrawCaret`, `DrawSelectionRect`, `DrawLine`

**Unrealised in BatchRenderer**:
- No `SetClip`/`RestoreClip` handling - scissor test never used
- No `DrawLine` primitive - only rectangles
- No `DrawCaret` - presumably drawn as thin rectangle elsewhere
- No `DrawDebugBorder` - presumably same as DrawRect

The BatchRenderer doesn't consume `DisplayList` or `PaintOp` objects at all. The paint operations layer exists but something else must be translating them to BatchRenderer calls.

### Asymmetric Batching Modes

**Declared (lines 22-23 .h)**: A begin/flush frame model suggesting all drawing batches uniformly.

**Unrealised (lines 243-291 .cpp)**: `drawImage()` completely bypasses the batch:
```cpp
void BatchRenderer::drawImage(...) {
    flush();  // Force flush pending batch
    // ... immediate mode draw with imageProg ...
}
```
Images are drawn immediately with their own shader, breaking the batching contract. This is workaround code - images can't be batched because they use external textures, but the API doesn't reflect this limitation.

### The `textured` Boolean

**Declared (line 56 .h)**: A boolean tracking "current batch mode" between textured and solid.

**Unrealised**: This is a two-state machine that should be an enum or the renderer should maintain separate batches. The current design forces a flush on every mode transition (lines 200-203, 224-226 .cpp):
```cpp
if (!textured && !vertices.empty()) {
    flush();
}
textured = true;
```
Alternating rect/text draws would flush on every call.

### Image Shader Reuse

**Declared (line 79-84 .cpp comment)**: "Image shader (reuses text.vert, different fragment shader)"

**Unrealised**: Despite the comment about reuse, a separate vertex shader is compiled (lines 83-85 .cpp):
```cpp
unsigned int ivs = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(ivs, 1, &textVSrc, nullptr);  // Reuse text vertex shader
glCompileShader(ivs);
```
The same source is compiled twice into two shader objects. This is unnecessary - the same compiled vertex shader could be attached to both programs.

### scrollOffsetY Parameter

**Declared (line 19 .h)**: `setViewport(int width, int height, float scrollOffsetY = 0.0f)`

**Partial realisation (lines 132-134 .cpp)**:
```cpp
// Apply vertical translation for scroll offset
projMatrix[13] += scrollOffsetY * projMatrix[5];
```
Scroll is baked into the projection matrix. This means scroll changes require recalculating the entire projection and all callers must re-call `setViewport`. A scroll offset should probably be a separate uniform or the caller's responsibility.

---

## Summary of Architectural Concerns

1. **Layer confusion**: BatchRenderer sits at the GPU abstraction layer but reaches up into text shaping (UTF-8 decode, glyph positioning) and down through font implementation (FT_Face)

2. **Unutilized abstraction**: `paint_operations.h` defines a display list model that BatchRenderer ignores - suggesting a missing intermediary that should translate PaintOps to batch calls

3. **Inconsistent batching**: `drawImage` breaks the batching model with immediate-mode rendering; `textured` boolean causes thrashing on mixed content

4. **Hidden assumptions**: The "immediate mode compatibility" state restoration (line 185 .cpp) compensates for an undocumented constraint about OpenGL state ownership
