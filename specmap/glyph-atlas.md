# Glyph Atlas Specification

## 1. Purpose and Overview

The Glyph Atlas (`glyph_atlas.cpp` / `glyph_atlas.h`) is a texture atlas system for efficient GPU-accelerated text rendering. It serves as a cache that stores rendered glyph bitmaps in a single OpenGL texture, enabling batched text rendering without per-glyph texture switches.

### Core Responsibilities

- Manage a single OpenGL texture containing all rendered glyphs
- Cache glyph metrics and UV coordinates for repeated use
- Provide on-demand glyph rasterization via FreeType
- Support multiple font variants (regular, bold, italic, monospace)

### Key Benefits

- **Performance**: Minimizes texture binds by packing glyphs into one texture
- **Memory efficiency**: Single allocation for the atlas texture
- **Batched rendering**: Enables the BatchRenderer to draw entire text runs in one draw call

## 2. Atlas Structure and Organization

### Texture Configuration

| Property | Value |
|----------|-------|
| Default dimensions | 1024 x 1024 pixels |
| Format | `GL_ALPHA` (single-channel, 8-bit) |
| Wrap mode | `GL_CLAMP_TO_EDGE` |
| Filtering | `GL_LINEAR` (both min and mag) |

The atlas uses a single-channel alpha texture because FreeType renders grayscale coverage values. The fragment shader samples this alpha and applies the text color.

### Shelf-Based Packing Algorithm

The atlas uses a **shelf packing** strategy (also known as "next-fit decreasing height" variant):

```
+------------------------------------------+
| Shelf 0 (shelfY=0, shelfH=max_glyph_h)   |
| [A][B][C][D]...                          |
+------------------------------------------+
| Shelf 1 (shelfY=prev_shelfY+shelfH+PAD)  |
| [E][F][G]...                             |
+------------------------------------------+
| (unused space)                           |
+------------------------------------------+
```

**Packing state variables**:

| Variable | Purpose |
|----------|---------|
| `shelfY` | Y-coordinate of current shelf's top edge |
| `shelfH` | Height of the tallest glyph in current shelf |
| `shelfX` | Next available X position in current shelf |
| `PAD` | 1-pixel padding between glyphs (prevents bleeding) |

**Algorithm**:

1. Check if glyph fits horizontally in current shelf (`shelfX + w + PAD <= atlasW`)
2. If not, start a new shelf (`shelfY += shelfH + PAD`, reset `shelfX = 0`)
3. If vertical space exhausted (`shelfY + h + PAD > atlasH`), atlas is full
4. Upload glyph bitmap via `glTexSubImage2D`
5. Calculate normalized UV coordinates
6. Advance `shelfX` by glyph width plus padding

## 3. Glyph Caching Strategy

### Cache Key Structure

```cpp
struct AtlasKey {
    uint32_t codepoint;  // Unicode codepoint
    int fontSize;        // Pixel size
    uint8_t style;       // TextStyle flags (Normal/Bold/Italic/Code)
    bool mono;           // Monospace font flag
};
```

The cache key uniquely identifies a glyph by its visual representation. The same codepoint at different sizes or styles produces different cache entries.

### Cache Storage

```cpp
std::map<AtlasKey, AtlasGlyph> cache;
```

The cache uses `std::map` with a custom comparison operator providing lexicographic ordering on (codepoint, fontSize, style, mono).

### Glyph Data Structure

```cpp
struct AtlasGlyph {
    float u0, v0, u1, v1;  // Normalized UV coordinates (0.0 to 1.0)
    int width, height;      // Bitmap dimensions in pixels
    int bearingX, bearingY; // Offset from pen position to top-left
    int advance;            // Horizontal advance to next glyph
};
```

### Lookup Flow

```
get(codepoint, fontSize, style, mono, face)
    |
    v
[Cache lookup] --> [Found] --> Return cached AtlasGlyph*
    |
    v [Not found]
    |
FT_Set_Pixel_Sizes(face, fontSize)
    |
    v
FT_Load_Char(face, codepoint, FT_LOAD_RENDER)
    |
    v
[Has bitmap?] --> [No] --> Store with zero UVs (space/empty glyph)
    |
    v [Yes]
    |
addToAtlas() --> [Full] --> Return nullptr
    |
    v [Success]
    |
Store in cache, return AtlasGlyph*
```

## 4. FreeType and HarfBuzz Integration

### FreeType Integration

The GlyphAtlas receives a pre-configured `FT_Face` from the Rasterizer and uses FreeType for:

1. **Size setting**: `FT_Set_Pixel_Sizes(face, 0, fontSize)` - sets character height in pixels
2. **Glyph loading**: `FT_Load_Char(face, codepoint, FT_LOAD_RENDER)` - loads and renders in one call
3. **Bitmap access**: `face->glyph->bitmap` provides the rendered coverage data

**Glyph metrics extraction**:

| FreeType Property | Atlas Usage |
|-------------------|-------------|
| `bitmap.width` | Glyph width in pixels |
| `bitmap.rows` | Glyph height in pixels |
| `bitmap_left` | `bearingX` - horizontal offset from pen |
| `bitmap_top` | `bearingY` - vertical offset from baseline |
| `advance.x >> 6` | Horizontal advance (converted from 26.6 fixed-point) |

### HarfBuzz Integration

HarfBuzz is **not directly used** by the GlyphAtlas. The atlas operates at the codepoint level, receiving Unicode codepoints from the calling code. Text shaping (complex script handling, ligatures, etc.) would occur at a higher level before calling `get()`.

**Note**: The current implementation uses simple codepoint-by-codepoint rendering in `BatchRenderer::drawText()`, which works for Latin scripts but would need HarfBuzz integration for proper complex script support.

## 5. Texture Management

### Initialization

```cpp
bool GlyphAtlas::init() {
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    // Pre-allocate with zeroed data
    std::vector<uint8_t> empty(atlasW * atlasH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlasW, atlasH, 0,
                 GL_ALPHA, GL_UNSIGNED_BYTE, empty.data());

    // Configure sampling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
```

### Glyph Upload

```cpp
glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // FreeType bitmaps are byte-aligned
glTexSubImage2D(GL_TEXTURE_2D, 0, shelfX, shelfY, w, h,
                GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);
```

`GL_UNPACK_ALIGNMENT = 1` is critical since FreeType bitmap rows are tightly packed without padding.

### UV Coordinate Calculation

UV coordinates are normalized to the 0.0-1.0 range:

```cpp
u0 = (float)shelfX / atlasW;        // Left edge
v0 = (float)shelfY / atlasH;        // Top edge
u1 = (float)(shelfX + w) / atlasW;  // Right edge
v1 = (float)(shelfY + h) / atlasH;  // Bottom edge
```

### Resource Cleanup

```cpp
GlyphAtlas::~GlyphAtlas() {
    if (texId) {
        glDeleteTextures(1, &texId);
    }
}
```

## 6. Font Variant Handling

### Style Flags

```cpp
enum class TextStyle : uint8_t {
    Normal = 0,
    Bold = 1 << 0,      // 0x01
    Italic = 1 << 1,    // 0x02
    Code = 1 << 2,      // 0x04
    BoldItalic = Bold | Italic  // 0x03
};
```

### Font Face Selection (in Rasterizer)

The `Rasterizer` maintains separate FreeType faces for each variant:

| Face Variable | Usage |
|---------------|-------|
| `faceRegular` | Normal text |
| `faceBold` | Bold text |
| `faceItalic` | Italic text |
| `faceBoldItalic` | Bold+Italic combined |
| `faceMono` | Monospace (code blocks) |

Selection logic:

```cpp
FT_Face getFaceForStyle(TextStyle style, bool monospace) {
    if (monospace && faceMono) return faceMono;

    if (hasStyle(style, Bold) && hasStyle(style, Italic))
        return faceBoldItalic;
    else if (hasStyle(style, Bold))
        return faceBold;
    else if (hasStyle(style, Italic))
        return faceItalic;

    return faceRegular;
}
```

### Platform-Specific Font Loading

| Platform | Regular Font | Variant Loading |
|----------|--------------|-----------------|
| macOS | System .ttc files (SF Pro, Helvetica Neue) | Face indices 0-3 within TTC |
| Linux | Bundled NotoSans-*.ttf | Separate font files |
| Web (Emscripten) | /fonts/NotoSans-*.ttf | Separate font files |

## 7. Notable Implementation Details

### Atlas Full Handling

When the atlas becomes full, `get()` returns `nullptr`. The current implementation has **no eviction or regrowth** strategy - once full, new glyphs cannot be added until the application restarts.

**Potential enhancement**: Implement LRU eviction or atlas regrowth for long-running sessions with diverse Unicode content.

### Empty Glyph Handling

Space characters and other glyphs without visual representation receive zero UV coordinates:

```cpp
if (g->bitmap.width > 0 && g->bitmap.rows > 0) {
    addToAtlas(...);
} else {
    glyph.u0 = glyph.v0 = glyph.u1 = glyph.v1 = 0;
}
```

These glyphs still participate in layout via their `advance` value.

### Thread Safety

The implementation is **not thread-safe**. All operations (cache lookup, FreeType calls, OpenGL uploads) must occur on the main/render thread.

### Memory Layout

- **CPU side**: `std::map` holding `AtlasGlyph` structs (~40 bytes per cached glyph)
- **GPU side**: Single 1024x1024 alpha texture (~1 MB VRAM)

### Integration with BatchRenderer

The `BatchRenderer` uses the atlas for text rendering:

1. Iterates through UTF-8 text, decoding codepoints
2. Calls `glyphAtlas->get()` for each codepoint
3. Positions quads using glyph metrics (bearingX, bearingY)
4. Uses UV coordinates to sample the atlas texture
5. Advances pen position by `glyph->advance`

```cpp
// In BatchRenderer::drawText
const AtlasGlyph* glyph = glyphAtlas->get(codepoint, fontSize, style, mono, face);
float xpos = penX + glyph->bearingX;
float ypos = baseline - glyph->bearingY;
drawQuad(xpos, ypos, glyph->width, glyph->height,
         glyph->u0, glyph->v0, glyph->u1, glyph->v1, ...);
penX += glyph->advance;
```

### OpenGL Compatibility

Uses OpenGL 2.1 / ES 2.0 compatible features:
- `GL_ALPHA` format (legacy, but widely supported)
- No VAOs (uses immediate-style attribute setup)
- Fixed function blending (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`)
