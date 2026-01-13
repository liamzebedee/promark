# Rasterizer Component Specification

## 1. Purpose and Overview

The Rasterizer is the rendering backend of the promark markdown editor. It serves as the bridge between the abstract paint operations (produced by the layout engine and painter) and the actual OpenGL rendering on screen.

### Key Responsibilities

- Execute paint operations from the display list
- Manage font loading and glyph rendering via FreeType
- Handle image loading, caching, and texture management
- Coordinate scroll offset and viewport transformations
- Orchestrate the batch renderer and glyph atlas subsystems

### Rendering Pipeline Flow

```
DisplayList (paint operations)
         |
         v
   Rasterizer.rasterize()
         |
         v
   BatchRenderer (geometry batching)
         |
         v
   GlyphAtlas (text glyph textures)
         |
         v
   OpenGL (GPU rendering)
```

---

## 2. Class Structure and OpenGL Setup

### Header: `rasterizer.h`

```cpp
class Rasterizer {
public:
    Rasterizer();
    ~Rasterizer();

    void rasterize(const DisplayList& displayList, const Rect& viewport,
                   float scrollOffsetY = 0.0f, bool caretVisible = true);
    bool initializeFont();

private:
    // Paint operation executors
    void executeDrawRect(const DrawRectOp& op);
    void executeDrawText(const DrawTextOp& op);
    void executeDrawImage(const DrawImageOp& op);
    void executeSetClip(const SetClipOp& op);
    void executeRestoreClip(const RestoreClipOp& op);
    void executeDrawDebugBorder(const DrawDebugBorderOp& op);
    void executeDrawCaret(const DrawCaretOp& op);
    void executeDrawSelectionRect(const DrawSelectionRectOp& op);
    void executeDrawLine(const DrawLineOp& op);

    // Image handling
    void loadImage(const std::string& imagePath);
    bool loadFromDataURI(const std::string& dataUri, ImageData& outData);
    bool decodeBase64(const std::string& base64, std::vector<uint8_t>& outBytes);
    bool decodePngFromMemory(const uint8_t* data, size_t length, ImageData& outData);
    bool decodeJpegFromMemory(const uint8_t* data, size_t length, ImageData& outData);

    // State
    std::map<std::string, ImageData> imageCache;
    Rect currentClip;
    bool hasClip;

    // FreeType font system
    FT_Library ft;
    FT_Face faceRegular, faceBold, faceItalic, faceBoldItalic, faceMono;
    bool fontLoaded;

    // GL2 renderer components
    std::unique_ptr<GlyphAtlas> atlas;
    std::unique_ptr<BatchRenderer> batchRenderer;
    bool gl2Initialized;
};
```

### ImageData Structure

```cpp
struct ImageData {
    uint32_t width, height;
    std::vector<uint8_t> pixels;  // RGBA pixel data
    uint32_t textureId;           // OpenGL texture handle
};
```

### Initialization Sequence

1. **Constructor**: Initializes font system, creates GlyphAtlas (1024x1024) and BatchRenderer
2. **Lazy GL Init**: On first `rasterize()` call, initializes OpenGL resources:
   - `atlas->init()` - Creates glyph texture
   - `batchRenderer->init()` - Compiles shaders, creates VBO

---

## 3. Shader Programs Used

The rasterizer uses three distinct shader programs, defined in `shaders_embedded.h`:

### 3.1 Text Shader (`textProg`)

Used for rendering text glyphs from the glyph atlas.

**Vertex Shader (`TEXT_VERT`):**
```glsl
attribute vec2 a_position;
attribute vec2 a_texcoord;
attribute vec4 a_color;
uniform mat4 u_projection;
varying vec2 v_texcoord;
varying vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}
```

**Fragment Shader (`TEXT_FRAG`):**
```glsl
varying vec2 v_texcoord;
varying vec4 v_color;
uniform sampler2D u_texture;

void main() {
    float alpha = texture2D(u_texture, v_texcoord).a;
    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
```

The text fragment shader samples only the alpha channel from the glyph atlas texture and multiplies it with the text color, enabling colored text with anti-aliased edges.

### 3.2 Solid Shader (`solidProg`)

Used for rendering solid-colored rectangles (backgrounds, selections, carets).

**Vertex Shader (`SOLID_VERT`):**
```glsl
attribute vec2 a_position;
attribute vec4 a_color;
uniform mat4 u_projection;
varying vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_color = a_color;
}
```

**Fragment Shader (`SOLID_FRAG`):**
```glsl
varying vec4 v_color;

void main() {
    gl_FragColor = v_color;
}
```

### 3.3 Image Shader (`imageProg`)

Used for rendering images with full RGBA texture sampling.

**Vertex Shader:** Reuses `TEXT_VERT`

**Fragment Shader (`IMAGE_FRAG`):**
```glsl
varying vec2 v_texcoord;
varying vec4 v_color;
uniform sampler2D u_texture;

void main() {
    vec4 texColor = texture2D(u_texture, v_texcoord);
    gl_FragColor = texColor * v_color;
}
```

### Platform-Specific Shader Preambles

```cpp
#ifdef __EMSCRIPTEN__
    const char* vertPreamble = "";
    const char* fragPreamble = "precision mediump float;\n";
#else
    const char* vertPreamble = "#version 120\n";
    const char* fragPreamble = "#version 120\n";
#endif
```

---

## 4. Rendering Different Element Types

### 4.1 Text Rendering (`executeDrawText`)

```cpp
void Rasterizer::executeDrawText(const DrawTextOp& op) {
    FT_Face face = getFaceForStyle(op.getStyle(), op.isMonospace());
    batchRenderer->drawText(text, position.x, position.y,
                            r, g, b, a, fontSize, style, monospace, face);
}
```

**Font Face Selection:**
- Monospace flag takes priority (uses `faceMono`)
- Otherwise selects based on TextStyle: Regular, Bold, Italic, or BoldItalic
- Falls back to regular face if specific style unavailable

**Text Drawing Process:**
1. Iterate through UTF-8 codepoints
2. Look up/render each glyph via GlyphAtlas
3. Position glyph using bearing offsets from baseline
4. Batch textured quads for efficient rendering

### 4.2 Rectangle Rendering (`executeDrawRect`)

```cpp
void Rasterizer::executeDrawRect(const DrawRectOp& op) {
    batchRenderer->drawRect(x, y, width, height, r, g, b, a);
}
```

Used for backgrounds, code blocks, and other solid fills. Colors are converted from 0-255 to 0.0-1.0 float range.

### 4.3 Selection Rectangles (`executeDrawSelectionRect`)

Identical to `drawRect`, renders semi-transparent highlight behind selected text.

### 4.4 Caret (Text Cursor) (`executeDrawCaret`)

```cpp
void Rasterizer::executeDrawCaret(const DrawCaretOp& op) {
    batchRenderer->drawRect(pos.x, pos.y, 2.0f, height, r, g, b, a);
}
```

Rendered as a 2-pixel wide rectangle. The `caretVisible` parameter in `rasterize()` controls blinking.

### 4.5 Lines (`executeDrawLine`)

```cpp
void Rasterizer::executeDrawLine(const DrawLineOp& op) {
    // Determine if primarily horizontal or vertical
    if (std::abs(dx) > std::abs(dy)) {
        // Horizontal line as thin rectangle
        batchRenderer->drawRect(minX, y - thickness/2, |dx|, thickness, ...);
    } else {
        // Vertical line
        batchRenderer->drawRect(x - thickness/2, minY, thickness, |dy|, ...);
    }
}
```

Lines are approximated as axis-aligned rectangles based on dominant direction.

### 4.6 Debug Borders (`executeDrawDebugBorder`)

Draws four thin rectangles (2px thickness) forming a border outline. Used for layout debugging.

### 4.7 Images (`executeDrawImage`)

```cpp
void Rasterizer::executeDrawImage(const DrawImageOp& op) {
    // Load and cache if needed
    if (imageCache.find(imagePath) == imageCache.end()) {
        loadImage(imagePath);
    }
    batchRenderer->drawImage(x, y, width, height, textureId);
}
```

**Image Loading Sources:**
1. **File paths**: Loaded via stb_image (PNG, JPEG)
2. **Data URIs**: Base64-decoded, then decoded by format

**Supported Formats:**
- PNG (via stb_image)
- JPEG (via libjpeg-turbo for memory decoding, stb_image for files)

**Fallback Behavior:** If image fails to load, creates a 100x100 gray placeholder texture.

### 4.8 Clipping (`executeSetClip` / `executeRestoreClip`)

```cpp
void Rasterizer::executeSetClip(const SetClipOp& op) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
}

void Rasterizer::executeRestoreClip(const RestoreClipOp& op) {
    glDisable(GL_SCISSOR_TEST);
}
```

Uses OpenGL scissor test for rectangular clipping regions.

---

## 5. Coordinate Transformations

### Orthographic Projection Matrix

The BatchRenderer constructs an orthographic projection with **top-left origin**:

```cpp
void BatchRenderer::setViewport(int width, int height, float scrollOffsetY) {
    // Orthographic projection (top-left origin)
    float l = 0, r = width, t = 0, b = height;
    float n = -1, f = 1;

    projMatrix[0] = 2.0f / (r - l);           // Scale X
    projMatrix[5] = 2.0f / (t - b);           // Scale Y (negative = flip)
    projMatrix[10] = -2.0f / (f - n);         // Scale Z
    projMatrix[12] = -(r + l) / (r - l);      // Translate X
    projMatrix[13] = -(t + b) / (t - b);      // Translate Y
    projMatrix[14] = -(f + n) / (f - n);      // Translate Z
    projMatrix[15] = 1.0f;

    // Apply scroll offset
    projMatrix[13] += scrollOffsetY * projMatrix[5];
}
```

### Coordinate System

| Property | Value |
|----------|-------|
| Origin | Top-left (0, 0) |
| X-axis | Right is positive |
| Y-axis | Down is positive |
| Z range | -1 to 1 (for 2D, Z=0) |

### Scroll Offset Handling

Scroll offset is applied as a vertical translation in the projection matrix, shifting all content up (negative Y direction when scrollOffsetY > 0).

---

## 6. Integration with Glyph Atlas and Batch Renderer

### GlyphAtlas

**Purpose:** Caches rendered glyph bitmaps in a single OpenGL texture for efficient text rendering.

**Key Features:**
- 1024x1024 pixel atlas texture (GL_ALPHA format)
- Shelf-packing algorithm for glyph placement
- Caches by: codepoint, fontSize, TextStyle, monospace flag

**Atlas Key Structure:**
```cpp
struct AtlasKey {
    uint32_t codepoint;
    int fontSize;
    uint8_t style;
    bool mono;
};
```

**Glyph Data:**
```cpp
struct AtlasGlyph {
    float u0, v0, u1, v1;  // UV coordinates (normalized)
    int width, height;      // Bitmap dimensions
    int bearingX, bearingY; // Offset from baseline
    int advance;            // Horizontal advance
};
```

**Glyph Retrieval Flow:**
1. Check cache by AtlasKey
2. If miss: render with FreeType, pack into atlas, cache
3. Return AtlasGlyph with UV coordinates

### BatchRenderer

**Purpose:** Batches draw calls to minimize OpenGL state changes and improve performance.

**Key Features:**
- Accumulates vertices up to 60,000 (10,000 quads)
- Automatic flush on capacity or shader switch
- Single VBO with dynamic updates

**Vertex Format:**
```cpp
struct Vertex {
    float x, y;       // Position (2D)
    float u, v;       // Texture coords
    float r, g, b, a; // Color
};
```

**Batching Strategy:**
- Solid geometry batched separately from textured
- Mode switch triggers flush
- Image draws are immediate (not batched) due to unique textures

**Integration Points:**
```cpp
// In Rasterizer::rasterize()
batchRenderer->setAtlas(atlas.get());  // Connect atlas
batchRenderer->setViewport(w, h, scrollY);
batchRenderer->begin();
// ... execute paint operations ...
batchRenderer->flush();
```

---

## 7. Notable Implementation Details

### 7.1 Lazy OpenGL Initialization

OpenGL resources are initialized on first use rather than in constructor:

```cpp
if (!gl2Initialized) {
    atlas->init();
    batchRenderer->init();
    batchRenderer->setAtlas(atlas.get());
    gl2Initialized = true;
}
```

This ensures the OpenGL context is available before resource creation.

### 7.2 Platform-Agnostic Font Loading

Fonts are loaded from platform-specific paths:

| Platform | Font Paths |
|----------|------------|
| Emscripten | `/fonts/NotoSans-*.ttf` |
| macOS | System fonts (Helvetica.ttc, Arial.ttf) |
| Linux | Bundled `fonts/NotoSans-*.ttf` |

**Fallback Chain:** If specific styles (bold/italic) fail to load, falls back to regular face.

### 7.3 Image Caching

Images are cached by path in `std::map<std::string, ImageData>`:
- Avoids reloading and re-uploading textures
- Supports both file paths and data URIs as keys
- Textures cleaned up in destructor

### 7.4 Data URI Support

Supports embedded images via data URIs:
```
data:image/png;base64,iVBORw0KGgo...
data:image/jpeg;base64,/9j/4AAQ...
```

Decoding pipeline:
1. Parse header to determine format
2. Base64 decode to binary
3. Decode image format (PNG/JPEG)
4. Upload to OpenGL texture

### 7.5 JPEG Memory Decoding

Uses libjpeg-turbo directly for in-memory JPEG decoding:

```cpp
jpeg_mem_src(&cinfo, data, length);
// ... decompress to RGB ...
// Convert RGB to RGBA (add alpha=255)
```

### 7.6 Blend Mode

All rendering uses standard alpha blending:
```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

### 7.7 Color Normalization

Paint operations use 0-255 integer colors; the rasterizer converts to 0.0-1.0 floats:
```cpp
color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f
```

### 7.8 Paint Operation Type Dispatch

The main rendering loop uses a switch statement on `PaintOpType`:

```cpp
for (const auto& op : displayList) {
    switch (op->getType()) {
        case PaintOpType::DrawRect: ...
        case PaintOpType::DrawText: ...
        case PaintOpType::DrawImage: ...
        case PaintOpType::SetClip: ...
        case PaintOpType::RestoreClip: ...
        case PaintOpType::DrawDebugBorder: ...
        case PaintOpType::DrawCaret: ...
        case PaintOpType::DrawSelectionRect: ...
        case PaintOpType::DrawLine: ...
    }
}
```

### 7.9 Resource Cleanup

The destructor properly cleans up all OpenGL and FreeType resources:
- Image textures from cache
- FreeType faces and library
- GlyphAtlas and BatchRenderer (via unique_ptr)

---

## File References

| File | Purpose |
|------|---------|
| `/protos/a/src/engine/rasterizer.h` | Class declaration |
| `/protos/a/src/engine/rasterizer.cpp` | Main implementation |
| `/protos/a/src/engine/batch_renderer.h` | BatchRenderer declaration |
| `/protos/a/src/engine/batch_renderer.cpp` | Batching and shader management |
| `/protos/a/src/engine/glyph_atlas.h` | GlyphAtlas declaration |
| `/protos/a/src/engine/glyph_atlas.cpp` | Glyph caching and atlas packing |
| `/protos/a/src/engine/shaders_embedded.h` | GLSL shader source code |
| `/protos/a/src/engine/paint_operations.h` | Paint operation definitions |
