# Design Analysis: GlyphAtlas

**Files:** `src/engine/glyph_atlas.h`, `src/engine/glyph_atlas.cpp`

---

## 1. Responsibilities

The GlyphAtlas module serves as a texture-backed glyph cache for GPU-accelerated text rendering:

1. **Glyph caching** - Maintains a map of rendered glyphs keyed by `(codepoint, fontSize, style, mono)` tuples (line 53, `.h`)
2. **Atlas texture management** - Allocates and owns a single OpenGL texture for all cached glyphs (lines 19-25, `.cpp`)
3. **Shelf-based packing** - Places glyph bitmaps into the atlas using a simple shelf algorithm (lines 82-118, `.cpp`)
4. **On-demand rasterization** - Renders glyphs via FreeType when cache misses occur (lines 50-64, `.cpp`)

---

## 2. Dependencies

### Direct Dependencies

| Dependency | Location | Purpose |
|------------|----------|---------|
| FreeType (`FT_Face`, `FT_Load_Char`, etc.) | `.h` lines 5-6, `.cpp` lines 50-64 | Font rasterization |
| OpenGL (`glTexImage2D`, `glTexSubImage2D`, etc.) | `.cpp` lines 19-32, 104-107 | GPU texture operations |
| `markdown_objects.h` | `.h` line 7 | `TextStyle` enum only |

### Dependency Analysis

**FreeType dependency is injected correctly** - The `FT_Face` is passed as a parameter to `get()` (line 36, `.h`), meaning GlyphAtlas does not own or manage font resources. However, it does directly call `FT_Set_Pixel_Sizes` and `FT_Load_Char` (lines 50-55, `.cpp`), coupling it to FreeType's API.

**OpenGL dependency is direct and unabstracted** - All GL calls are made inline without any abstraction layer. This couples the module to OpenGL 2.1 (uses `GL_ALPHA` format, deprecated in later versions).

**markdown_objects.h import is questionable** - Only `TextStyle` is used from this 223-line file. The enum is a rendering concern but lives in a "markdown objects" module, creating an awkward dependency.

---

## 3. Mutation Points

### Internal State Modified

| State | Mutated By | Notes |
|-------|------------|-------|
| `texId` | `init()`, destructor | GPU texture handle |
| `cache` | `get()` | Glyph metadata cache |
| `shelfX`, `shelfY`, `shelfH` | `addToAtlas()` | Packing cursor state |
| `initialized` | `init()` | Initialization flag |

### External State Modified

| State | Location | Issue |
|-------|----------|-------|
| OpenGL texture memory | `init()`, `addToAtlas()` | Direct GL state mutation |
| OpenGL pixel store state | `addToAtlas()` line 105 | Sets `GL_UNPACK_ALIGNMENT` but never restores |
| OpenGL bind state | `init()`, `addToAtlas()`, `bind()` | Changes `GL_TEXTURE_2D` binding |

### Authority Concerns

**The atlas modifies font state it doesn't own.** At line 50, `get()` calls `FT_Set_Pixel_Sizes(face, ...)` on a face that was passed in. This means:
- The caller's FT_Face state is silently modified
- Two GlyphAtlas instances sharing an FT_Face would corrupt each other's font size
- The atlas assumes exclusive control over font sizing

**GL state leakage.** `addToAtlas()` sets `GL_UNPACK_ALIGNMENT` to 1 (line 105) but never restores it. This could affect other GL operations expecting the default alignment of 4.

---

## 4. Boundary Violations

### Layer Inversion: Domain Type in Rendering Module

```cpp
// glyph_atlas.h:7
#include "markdown_objects.h"
```

The glyph atlas is a low-level rendering primitive. It should not depend on `markdown_objects.h`, which defines document-level abstractions like `HeadingObject`, `ListObject`, `TableObject`, etc. The dependency exists solely for the `TextStyle` enum (lines 42-48 of markdown_objects.h).

**Impact:** Changes to the markdown object model force recompilation of the glyph atlas.

### FreeType Operations in Wrong Layer

```cpp
// glyph_atlas.cpp:50-55
FT_Set_Pixel_Sizes(face, 0, fontSize);
if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
    return nullptr;
}
```

The GlyphAtlas performs font rasterization directly. This mixes two concerns:
1. Atlas management (texture packing, UV calculation, caching)
2. Glyph rasterization (FreeType calls)

A cleaner design would have the atlas receive pre-rendered bitmap data rather than an FT_Face.

---

## 5. Declared-but-Unrealised Design

### AtlasKey `style` Field is Stored but Not Used for Font Selection

```cpp
// glyph_atlas.h:16-28
struct AtlasKey {
    uint32_t codepoint;
    int fontSize;
    uint8_t style;      // <-- Stored in key
    bool mono;

    bool operator<(const AtlasKey& o) const { ... }
};
```

The `style` field is part of the cache key, implying different glyphs for Bold/Italic/Normal. However, the `get()` function:

1. Accepts `style` as a parameter (line 36)
2. Stores it in the key (line 42, `.cpp`)
3. **Never uses it to affect rasterization** - The same `FT_Face` and `FT_Load_Char` call is used regardless of style

The actual style differentiation happens in `Rasterizer::executeDrawText()` (line 118, rasterizer.cpp), which selects the appropriate `FT_Face` (faceRegular, faceBold, faceItalic, faceBoldItalic) and passes it to `BatchRenderer::drawText()`.

**The style field in AtlasKey serves only as a cache discriminator, not as a rendering directive.** This is correct but the naming implies the atlas handles style, when it actually doesn't.

### `mono` Flag Pattern Matches `style` Issue

The `mono` boolean follows the same pattern - it's stored in the key but the actual monospace face selection happens in the Rasterizer, not the atlas.

### Atlas-Full Handling is Incomplete

```cpp
// glyph_atlas.cpp:70-72
if (!addToAtlas(...)) {
    // Atlas full
    return nullptr;
}
```

When the atlas fills up, `get()` returns `nullptr` and the caller silently skips the glyph. There is:
- No atlas expansion/recreation
- No eviction policy (LRU, etc.)
- No error logging or callback
- No mechanism to inform higher layers

The comment "Atlas full" acknowledges this but provides no solution. The caller (`BatchRenderer::drawText()` line 307) simply continues: `if (!glyph) continue;`

### Multiple Atlas Instances Exist

```cpp
// engine.cpp:1223, 1459
uiAtlas = std::make_unique<GlyphAtlas>(512, 512);

// rasterizer.cpp:22
atlas = std::make_unique<GlyphAtlas>(1024, 1024);
```

Two separate GlyphAtlas instances exist with different sizes (512x512 and 1024x1024). This suggests an unrealised design for separating UI text from document text, but:
- They share no glyphs (duplicate rasterization)
- They have independent caches (memory waste)
- The size difference implies different usage patterns but this isn't documented

### `bind()` is a No-Op Pattern

```cpp
// glyph_atlas.cpp:37-39
void GlyphAtlas::bind() {
    glBindTexture(GL_TEXTURE_2D, texId);
}
```

This method exists but is never called in the codebase. The `BatchRenderer` uses `textureId()` accessor instead and manages binding itself. The `bind()` method represents an unrealised abstraction where the atlas would own its own binding lifecycle.

---

## Summary of Architectural Concerns

1. **Inappropriate dependency on markdown_objects.h** - Should extract `TextStyle` to a separate types header
2. **FreeType coupling** - Atlas conflates caching with rasterization; should receive pre-rendered bitmaps
3. **Mutates foreign state** - `FT_Set_Pixel_Sizes` on caller's FT_Face, `GL_UNPACK_ALIGNMENT` not restored
4. **No atlas overflow strategy** - Silent failure when full, no eviction or expansion
5. **Duplicate instances** - Two atlases in the codebase with no sharing mechanism
6. **Dead code** - `bind()` method is never used
