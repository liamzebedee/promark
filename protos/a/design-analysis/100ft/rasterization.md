# Rasterization Subsystem: Compressed Design Analysis

**Source documents:** `designs/engine/rasterizer.md`, `batch_renderer.md`, `glyph_atlas.md`, `shaders.md`

---

## Architecture Overview

The rasterization pipeline flows: **DisplayList -> Rasterizer -> BatchRenderer -> Shaders -> GPU**

The Rasterizer interprets paint operations, owns font/image resources, and coordinates two subsystems:
- **GlyphAtlas**: Texture-backed glyph cache with shelf packing
- **BatchRenderer**: Batches draw calls, owns shader programs

Three shader programs exist: `textProg` (glyph atlas), `solidProg` (rectangles), `imageProg` (external textures).

---

## Cross-Cutting Concern: Split GL Authority

**The central architectural flaw.** OpenGL state is mutated by multiple components without coordination:

| Component | GL State Modified | Issue |
|-----------|-------------------|-------|
| Rasterizer | Scissor test, texture creation/binding | Bypasses BatchRenderer |
| BatchRenderer | Blend mode, bound texture, active program, vertex attribs | Assumes exclusive ownership |
| GlyphAtlas | `GL_UNPACK_ALIGNMENT`, texture binding | Never restores alignment |

The BatchRenderer was designed to be the sole GL abstraction, but the Rasterizer makes direct calls for scissor testing (lines 151-159) and image textures (lines 190-196). This creates:
- State desync potential between components
- "Immediate mode compatibility" workarounds (BatchRenderer line 185)
- Defensive state restoration after every operation

**Evidence of instability:** The comment "restore state for immediate mode compatibility" in BatchRenderer acknowledges an undocumented constraint about who owns GL state.

---

## Cross-Cutting Concern: FreeType Leakage

FreeType types appear at every layer boundary, violating encapsulation:

```
Rasterizer.h    -> exposes FT_Library, FT_Face members (lines 51-57)
BatchRenderer.h -> FT_Face in drawText signature (line 36)
GlyphAtlas.h    -> FT_Face parameter to get() (line 36)
```

The atlas modifies foreign state: `FT_Set_Pixel_Sizes(face, ...)` is called on a face it doesn't own (glyph_atlas.cpp:50). Two atlas instances sharing an FT_Face would corrupt each other's font size.

**Design intent:** The layers suggest font details should be hidden. **Reality:** FT_Face is threaded through every component.

---

## Cross-Cutting Concern: markdown_objects.h Dependency

Both GlyphAtlas and BatchRenderer import `markdown_objects.h` (223 lines of document model) solely for the `TextStyle` enum (4 values: Normal, Bold, Italic, BoldItalic).

This couples low-level rendering primitives to the document model. Changes to markdown object definitions force recompilation of the entire rasterization stack.

---

## Unstable Boundary: Text Shaping Responsibility

Text shaping logic is split awkwardly:

| Concern | Located In | Should Be In |
|---------|------------|--------------|
| Font face selection (bold/italic) | Rasterizer.executeDrawText | OK |
| UTF-8 decoding | BatchRenderer.drawText | Text layer above |
| Glyph positioning (penX += advance) | BatchRenderer.drawText | Text layer above |
| Glyph rasterization | GlyphAtlas.get | OK for caching |

The BatchRenderer reaches up into text shaping (lines 298-319) while it should only receive pre-positioned glyphs. It imports utf8.h and performs horizontal layout in the rendering layer.

---

## Unstable Boundary: Image Loading vs Rendering

Image loading is embedded in the Rasterizer (lines 162-335):
- Base64 decoding
- PNG decoding via stb_image
- JPEG decoding via libjpeg (memory path) or stb_image (file path)
- Texture creation

This conflates resource loading with rendering. The asymmetric codec usage (libjpeg for memory, stb_image for files) reveals two implementation approaches never unified.

---

## Paper Abstraction: PaintOp Display List

**Declared:** `paint_operations.h` defines a comprehensive command protocol:
- DrawRect, DrawText, DrawImage, DrawLine
- SetClip, RestoreClip
- DrawCaret, DrawDebugBorder, DrawSelectionRect

**Unrealised:** BatchRenderer ignores this entirely. It provides `drawQuad`, `drawRect`, `drawImage`, `drawText` - raw primitives, not command interpreters. The Rasterizer manually dispatches each PaintOp to BatchRenderer calls.

The display list abstraction exists structurally but provides no value - the Rasterizer switch statement (lines 45-94) is the real protocol implementation.

---

## Paper Abstraction: Batching

**Declared:** begin/flush frame model for batched rendering.

**Unrealised in two ways:**

1. **drawImage bypasses batching** (batch_renderer.cpp:243-291): Calls `flush()` immediately, then does immediate-mode rendering. Images can't batch because they use external textures, but the API doesn't reflect this.

2. **textured boolean causes thrashing**: Alternating rect/text calls flush on every mode transition:
   ```cpp
   if (!textured && !vertices.empty()) { flush(); }
   textured = true;
   ```
   A document with mixed content defeats batching.

---

## Paper Abstraction: Atlas Style/Mono Fields

**Declared:** `AtlasKey` includes `style` and `mono` fields, implying the atlas handles font variants.

**Unrealised:** The atlas never uses these for rasterization. They serve only as cache discriminators. The actual font face selection happens in the Rasterizer, which passes the appropriate FT_Face. The naming implies capability that doesn't exist.

---

## Paper Abstraction: image.frag Tinting

**Declared:** `gl_FragColor = texColor * v_color` enables color tinting.

**Unrealised:** BatchRenderer always passes white `(1,1,1,1)`. The multiplication is a no-op. Capabilities exist in the shader for dimming, overlays, and fades, but no API exposes them.

---

## Repeated Pattern: Two-Phase Initialization

Multiple components defer initialization until GL context is ready:

- Rasterizer: `gl2Initialized` flag, lazy init in first `rasterize()` call
- GlyphAtlas: `initialized` flag, explicit `init()` method
- BatchRenderer: `init()` method called by Rasterizer

This pattern exists because objects are constructed before GL context availability. A cleaner design would construct these objects when the GL context is ready.

---

## Repeated Pattern: Silent Failure

Errors are swallowed without notification:

| Location | Failure Mode | Handling |
|----------|--------------|----------|
| Atlas full | `get()` returns nullptr | `drawText` skips glyph silently |
| Font load failure | Face aliased to regular | Appears to work with wrong font |
| Glyph rasterization failure | `FT_Load_Char` returns error | Returns nullptr, skipped |

No eviction policy exists for full atlases. No logging or callbacks for failures.

---

## Repeated Pattern: Duplicate Resources

Two GlyphAtlas instances exist:
- engine.cpp: 512x512 for UI text
- rasterizer.cpp: 1024x1024 for document text

No glyph sharing between them. Common glyphs are rasterized twice, cached twice.

---

## Repeated Pattern: Dead/Stub Code

| Location | Status |
|----------|--------|
| `Rasterizer::decodeJpeg(filepath)` | Empty TODO |
| `Rasterizer::decodePng(filepath)` | Empty TODO |
| `GlyphAtlas::bind()` | Implemented but never called |
| `image.frag v_color` | Shader supports it, API doesn't use it |

---

## Shader Architecture Notes

**Vertex-Fragment Coupling:**
- `imageProg` silently reuses `text.vert` (comment at batch_renderer.cpp:83)
- No `image.vert` exists on disk; the dependency is implicit
- Modifying `text.vert` can break image rendering without obvious connection

**Wasted Bandwidth:**
- `solid.vert` ignores texcoords but `drawRect` sends them anyway
- Vertex struct is uniform regardless of shader requirements

**Hardcoded Limitations:**
- Z=0 hardcoded in vertex shaders prevents depth ordering
- GLSL version injected at runtime; raw .frag/.vert files are invalid standalone
- No gamma correction anywhere in the pipeline

---

## Severity Summary

| Issue | Impact |
|-------|--------|
| Split GL authority | Active bugs, state desync |
| FreeType leakage | Compilation coupling, potential race |
| Text shaping in renderer | Layer violation, hard to test |
| PaintOp abstraction unused | Structural complexity, no benefit |
| Silent failures | Debugging difficulty |
| Image batching bypass | Performance on image-heavy docs |
