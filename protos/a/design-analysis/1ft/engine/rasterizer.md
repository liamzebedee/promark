# Rasterizer Design Analysis

**Files:** `src/engine/rasterizer.h`, `src/engine/rasterizer.cpp`

---

## 1. Responsibilities

The Rasterizer module is the **final rendering stage** in the paint pipeline. It consumes a `DisplayList` (vector of paint operations) and produces OpenGL draw calls.

### Core Responsibilities

1. **Display List Execution** (lines 45-94 in .cpp)
   - Iterates over `DisplayList` and dispatches each `PaintOp` to the appropriate execute handler
   - Acts as command interpreter for the paint operation protocol

2. **Font Management** (lines 341-454 in .cpp)
   - Owns FreeType library instance (`ft`)
   - Loads and manages font faces: regular, bold, italic, bold-italic, monospace
   - Platform-specific font path resolution (Emscripten, macOS, Linux)

3. **Image Loading and Caching** (lines 162-335 in .cpp)
   - Maintains `imageCache` mapping paths to `ImageData`
   - Decodes PNG/JPEG from files and data URIs
   - Creates and manages OpenGL textures for images

4. **GL State Orchestration**
   - Coordinates `GlyphAtlas` and `BatchRenderer` lifecycle
   - Manages scissor test for clipping (lines 146-160)

---

## 2. Dependencies

### Direct Dependencies (Header)

| Dependency | Purpose | Concern |
|------------|---------|---------|
| `paint_operations.h` | DisplayList, PaintOp types | Correct - input protocol |
| `glyph_atlas.h` | Text glyph caching | Correct - owned subsystem |
| `batch_renderer.h` | GPU draw batching | Correct - owned subsystem |
| `<ft2build.h>`, `FT_FREETYPE_H` | Font rasterization | **Leaky** - FreeType types in header |
| `<jpeglib.h>` | JPEG decoding | **Unnecessary** - only used in .cpp |

### Implicit Dependencies (Implementation)

| Dependency | Location | Concern |
|------------|----------|---------|
| `gl_includes.h` | line 3 | Direct GL calls bypass BatchRenderer |
| `stb/stb_image.h` | line 13 | STB_IMAGE_IMPLEMENTATION defined here |
| `utf8.h` | line 2 | UTF-8 iteration (used by BatchRenderer, not directly) |

### Dependency Direction Issues

The Rasterizer includes `<jpeglib.h>` in the **header** (line 11) despite JPEG decoding being entirely internal to the implementation. This pollutes the include chain for any consumer of `rasterizer.h`.

---

## 3. Mutation Points

### Owned State

| State | Location | Mutated By |
|-------|----------|------------|
| `imageCache` | line 46 | `loadImage()` - grows on cache miss |
| `currentClip` / `hasClip` | lines 47-48 | `executeSetClip()`, `executeRestoreClip()` |
| `ft`, `face*` | lines 51-57 | `initializeFont()`, destructor |
| `atlas` | line 66 | Constructor, `rasterize()` for lazy init |
| `batchRenderer` | line 67 | Constructor, `rasterize()` for lazy init |
| `gl2Initialized` | line 68 | `rasterize()` - one-shot flag |

### External State Mutation

| State | Location | Concern |
|-------|----------|---------|
| OpenGL scissor state | lines 151-159 | `glEnable/glDisable(GL_SCISSOR_TEST)` - **bypasses BatchRenderer** |
| OpenGL textures | lines 190-196, 28-31 | Direct `glGenTextures`, `glDeleteTextures` |
| Bound texture | lines 191, 196 | `glBindTexture` - **can desync BatchRenderer state** |

### Authority Confusion

The `BatchRenderer` exists to abstract GL state, but the Rasterizer makes direct GL calls for:
- Scissor testing (lines 151-159)
- Texture creation/deletion (lines 190-196, 28-31)

This creates a **split authority** problem where GL state can be modified by either component, requiring careful synchronization that is not explicitly managed.

---

## 4. Boundary Violations

### Direct OpenGL Calls

```cpp
// rasterizer.cpp:151-153
glEnable(GL_SCISSOR_TEST);
glScissor(currentClip.position.x, currentClip.position.y,
          currentClip.size.width, currentClip.size.height);
```

```cpp
// rasterizer.cpp:159
glDisable(GL_SCISSOR_TEST);
```

```cpp
// rasterizer.cpp:190-196
glGenTextures(1, &imgData.textureId);
glBindTexture(GL_TEXTURE_2D, imgData.textureId);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ...);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glBindTexture(GL_TEXTURE_2D, 0);
```

These violate the intended layering where `BatchRenderer` should be the **sole** point of GL interaction for rendering. The Rasterizer should delegate all GL operations.

### FreeType Leakage

The header exposes FreeType types:
```cpp
// rasterizer.h:51-56
FT_Library ft;
FT_Face faceRegular;
FT_Face faceBold;
// ...
```

This forces all consumers to include FreeType headers. Font management should be encapsulated behind an interface or forward declarations.

### Image Loading Coupled to Rasterizer

Image decoding logic (base64, PNG, JPEG - lines 162-335) is embedded directly in the Rasterizer. This violates single responsibility; image loading should be a separate utility that produces texture data, not a Rasterizer concern.

---

## 5. Declared-but-Unrealised Design

### Stubbed Methods

```cpp
// rasterizer.h:39-40
void decodeJpeg(const std::string& filePath);
void decodePng(const std::string& filePath);
```

```cpp
// rasterizer.cpp:285-288
void Rasterizer::decodeJpeg(const std::string& filePath) {
    // TODO: Implement JPEG file decoding
}
```

```cpp
// rasterizer.cpp:337-339
void Rasterizer::decodePng(const std::string& filePath) {
    // TODO: Implement PNG decoding
}
```

These methods are declared in the header, suggesting a design where file-based decoding would use these methods. Instead, `loadImage()` (line 162) directly uses `stbi_load()` for file paths, bypassing the declared interface entirely.

### Asymmetric Image Decoding

| Format | From File | From Memory |
|--------|-----------|-------------|
| PNG | `stbi_load()` via `loadImage()` | `decodePngFromMemory()` via stb_image |
| JPEG | `stbi_load()` via `loadImage()` | `decodeJpegFromMemory()` via libjpeg |

The in-memory JPEG path uses libjpeg directly (lines 289-335) while file-based loading uses stb_image. This inconsistency suggests two different implementation approaches that were never unified.

### Lazy Initialization Pattern

```cpp
// rasterizer.cpp:46-52
if (!gl2Initialized) {
    atlas->init();
    batchRenderer->init();
    batchRenderer->setAtlas(atlas.get());
    gl2Initialized = true;
}
```

The objects are created in the constructor (lines 22-23) but initialized on first `rasterize()` call. This two-phase initialization compensates for OpenGL context not being available at construction time. The design implies these objects should be created when the GL context is ready, not at Rasterizer construction.

### Font Fallback Compensation

```cpp
// rasterizer.cpp:404-407
if (!faceBold) faceBold = faceRegular;
if (!faceItalic) faceItalic = faceRegular;
if (!faceBoldItalic) faceBoldItalic = faceBold ? faceBold : faceRegular;
```

The font system declares separate faces for each style but compensates for missing faces by aliasing to regular. This workaround hides font loading failures from callers and makes debugging style issues difficult.

### Caret Visibility as Rasterizer Concern

```cpp
// rasterizer.cpp:79-82
if (caretVisible) {
    executeDrawCaret(static_cast<const DrawCaretOp&>(*op));
}
```

The `rasterize()` method takes a `caretVisible` parameter (line 45) to conditionally skip caret rendering. This pushes UI state (blink phase) into the rendering layer. The Painter should omit the `DrawCaretOp` when invisible, not push this decision to the Rasterizer.

---

## Summary of Architectural Issues

1. **Split GL Authority**: Rasterizer makes direct GL calls that should go through BatchRenderer
2. **Header Pollution**: FreeType and jpeglib types exposed in public header
3. **Embedded Image Loading**: Should be extracted to a separate ImageLoader utility
4. **Dead Code**: `decodeJpeg()` and `decodePng()` file methods are stubs
5. **Two-Phase Init**: Lazy GL initialization compensates for construction timing
6. **Leaked UI State**: `caretVisible` parameter pushes presentation logic into rendering
7. **Inconsistent Codec Usage**: JPEG uses libjpeg for memory, stb_image for files
