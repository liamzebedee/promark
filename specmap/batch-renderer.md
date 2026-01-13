# Batch Renderer Specification

## 1. Purpose and Overview

The `BatchRenderer` is a core rendering component of the promark markdown editor responsible for efficiently drawing all 2D graphics to the screen using OpenGL. It implements a batched rendering approach that minimizes GPU state changes and draw calls by grouping similar primitives together before submission.

**Key Responsibilities:**
- Accumulate geometry (quads, rectangles, text glyphs) into batches
- Manage shader programs for different rendering modes (solid, textured, image)
- Handle orthographic projection with scroll offset support
- Coordinate with the glyph atlas for text rendering
- Provide a clean API for drawing primitives

**Location:** `/home/liam/Documents/projects/promark/protos/a/src/engine/batch_renderer.cpp`

## 2. Batching Strategy and Optimization

### 2.1 Core Batching Concept

The renderer accumulates vertex data into a single buffer and submits it in one draw call when:
1. The buffer reaches capacity (60,000 vertices / 10,000 quads)
2. A render mode switch occurs (textured <-> solid)
3. `flush()` is explicitly called (e.g., end of frame)

### 2.2 Mode-Based Batching

The renderer tracks a `textured` boolean state that determines whether the current batch uses the text shader (with glyph atlas texture) or the solid shader (untextured geometry).

```
Solid batch:     [rect][rect][rect][selection][caret]  -> single draw call
Textured batch:  [glyph][glyph][glyph][glyph]          -> single draw call
```

When switching between modes, the current batch is flushed before starting a new one:

| Current Mode | New Primitive | Action |
|--------------|---------------|--------|
| Solid | `drawRect()` | Continue accumulating |
| Solid | `drawQuad()` (textured) | Flush, switch to textured |
| Textured | `drawQuad()` | Continue accumulating |
| Textured | `drawRect()` | Flush, switch to solid |

### 2.3 Capacity Management

```cpp
static constexpr size_t MAX_VERTICES = 60000;  // 10,000 quads * 6 vertices

void ensureCapacity(size_t numVertices) {
    if (vertices.size() + numVertices > MAX_VERTICES) {
        flush();
    }
}
```

The buffer is pre-allocated with `GL_DYNAMIC_DRAW` hint for efficient streaming updates.

### 2.4 Image Rendering Exception

Images bypass batching entirely. Each `drawImage()` call:
1. Flushes pending geometry
2. Binds the image's unique texture
3. Draws a single quad immediately
4. Does not affect the `textured` state

This is necessary because images have individual textures, unlike text glyphs which share the atlas.

## 3. Vertex Data Structures

### 3.1 Vertex Format

```cpp
struct Vertex {
    float x, y;       // Position (2D screen coordinates)
    float u, v;       // Texture coordinates (normalized 0-1)
    float r, g, b, a; // Color (normalized 0-1)
};
```

**Size:** 32 bytes per vertex (8 floats * 4 bytes)

**Total Buffer Size:** 60,000 vertices * 32 bytes = 1.875 MB

### 3.2 Attribute Layout

| Attribute | Location | Components | Type | Offset |
|-----------|----------|------------|------|--------|
| `a_position` | 0 | 2 (x, y) | float | 0 |
| `a_texcoord` | 1 | 2 (u, v) | float | 8 |
| `a_color` | 2 | 4 (r, g, b, a) | float | 16 |

### 3.3 Quad Construction

Each quad (rectangle or textured glyph) is decomposed into 2 triangles using 6 vertices:

```
Triangle 1: top-left, top-right, bottom-right
Triangle 2: top-left, bottom-right, bottom-left

  0---1          Vertex order:
  |  /|          0: (x, y)      top-left
  | / |          1: (x+w, y)    top-right
  |/  |          2: (x+w, y+h)  bottom-right
  3---2          (repeat 0, 2 for triangle 2)
                 3: (x, y+h)    bottom-left
```

## 4. Shader Management

### 4.1 Shader Programs

Three distinct shader programs are compiled during initialization:

| Program | Vertex Shader | Fragment Shader | Purpose |
|---------|---------------|-----------------|---------|
| `textProg` | `TEXT_VERT` | `TEXT_FRAG` | Glyph rendering with alpha from texture |
| `solidProg` | `SOLID_VERT` | `SOLID_FRAG` | Solid color rectangles |
| `imageProg` | `TEXT_VERT` | `IMAGE_FRAG` | Full RGBA image rendering |

### 4.2 Platform-Specific Preambles

Shaders are embedded as raw GLSL strings and prefixed with platform-appropriate preambles:

```cpp
#ifdef __EMSCRIPTEN__
    vertPreamble = "";
    fragPreamble = "precision mediump float;\n";
#else
    vertPreamble = "#version 120\n";
    fragPreamble = "#version 120\n";
#endif
```

### 4.3 Shader Sources

**Text Vertex Shader (`TEXT_VERT`):**
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

**Text Fragment Shader (`TEXT_FRAG`):**
```glsl
uniform sampler2D u_texture;
varying vec2 v_texcoord;
varying vec4 v_color;

void main() {
    float alpha = texture2D(u_texture, v_texcoord).a;
    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
```

The text shader samples only the alpha channel from the glyph atlas (GL_ALPHA format) and multiplies it with the vertex color, enabling per-glyph coloring.

**Solid Fragment Shader (`SOLID_FRAG`):**
```glsl
varying vec4 v_color;

void main() {
    gl_FragColor = v_color;
}
```

**Image Fragment Shader (`IMAGE_FRAG`):**
```glsl
uniform sampler2D u_texture;
varying vec2 v_texcoord;
varying vec4 v_color;

void main() {
    vec4 texColor = texture2D(u_texture, v_texcoord);
    gl_FragColor = texColor * v_color;
}
```

### 4.4 Uniform Handling

| Uniform | Type | Description |
|---------|------|-------------|
| `u_projection` | mat4 | Orthographic projection matrix |
| `u_texture` | sampler2D | Texture unit 0 (atlas or image) |

## 5. Draw Call Organization

### 5.1 Frame Lifecycle

```
begin()                    // Clear vertex buffer
  drawRect(...)            // Accumulate solid geometry
  drawRect(...)
  drawText(...)            // [FLUSH] switch to textured, accumulate glyphs
  drawText(...)
  drawRect(...)            // [FLUSH] switch to solid
  drawImage(...)           // [FLUSH] immediate draw with unique texture
  drawRect(...)            // Continue solid batch
flush()                    // Submit final batch
```

### 5.2 Projection Matrix

The renderer uses a top-left origin orthographic projection suitable for 2D UI:

```cpp
// Orthographic projection setup
float l = 0, r = width, t = 0, b = height;
float n = -1, f = 1;

projMatrix[0] = 2.0f / (r - l);           // X scale
projMatrix[5] = 2.0f / (t - b);           // Y scale (flipped for top-left origin)
projMatrix[10] = -2.0f / (f - n);         // Z scale
projMatrix[12] = -(r + l) / (r - l);      // X translation
projMatrix[13] = -(t + b) / (t - b);      // Y translation
projMatrix[14] = -(f + n) / (f - n);      // Z translation
projMatrix[15] = 1.0f;
```

Scroll offset is applied as a Y-axis translation: `projMatrix[13] += scrollOffsetY * projMatrix[5]`

### 5.3 Flush Implementation

```cpp
void BatchRenderer::flush() {
    if (vertices.empty()) return;

    // 1. Upload vertex data
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());

    // 2. Select shader program based on mode
    unsigned int prog = textured ? textProg : solidProg;
    glUseProgram(prog);

    // 3. Set uniforms (projection, texture)
    // 4. Configure vertex attributes
    // 5. Enable blending (SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
    // 6. Draw triangles
    glDrawArrays(GL_TRIANGLES, 0, vertices.size());

    // 7. Cleanup and clear buffer
    vertices.clear();
}
```

## 6. Integration with Rasterizer

### 6.1 Initialization Sequence

The `Rasterizer` owns both the `GlyphAtlas` and `BatchRenderer`:

```cpp
// In Rasterizer constructor
atlas = std::make_unique<GlyphAtlas>(1024, 1024);
batchRenderer = std::make_unique<BatchRenderer>();

// Lazy initialization (requires GL context)
void Rasterizer::rasterize(...) {
    if (!gl2Initialized) {
        atlas->init();
        batchRenderer->init();
        batchRenderer->setAtlas(atlas.get());
        gl2Initialized = true;
    }
    // ...
}
```

### 6.2 Display List Execution

The rasterizer translates paint operations into batch renderer calls:

| Paint Operation | Batch Renderer Method |
|-----------------|----------------------|
| `DrawRectOp` | `drawRect()` |
| `DrawTextOp` | `drawText()` |
| `DrawImageOp` | `drawImage()` |
| `DrawDebugBorderOp` | 4x `drawRect()` (border edges) |
| `DrawCaretOp` | `drawRect()` (2px wide) |
| `DrawSelectionRectOp` | `drawRect()` |
| `DrawLineOp` | `drawRect()` (thin rectangle) |

### 6.3 Color Conversion

Paint operations use `Color` (0-255 uint8), which rasterizer converts to normalized floats:

```cpp
batchRenderer->drawRect(...,
    color.r / 255.0f, color.g / 255.0f,
    color.b / 255.0f, color.a / 255.0f);
```

## 7. Notable Implementation Details

### 7.1 Text Rendering Pipeline

The `drawText()` method handles UTF-8 decoding and glyph positioning:

```cpp
void BatchRenderer::drawText(const std::string& text, float x, float y, ...) {
    float penX = x;
    float baseline = y;

    size_t pos = 0;
    while (pos < text.length()) {
        uint32_t codepoint = utf8::decode(text, pos);
        if (codepoint == '\n') continue;

        const AtlasGlyph* glyph = glyphAtlas->get(codepoint, fontSize, style, mono, face);
        if (!glyph) continue;

        if (glyph->width > 0 && glyph->height > 0) {
            float xpos = penX + glyph->bearingX;
            float ypos = baseline - glyph->bearingY;
            drawQuad(xpos, ypos, glyph->width, glyph->height,
                     glyph->u0, glyph->v0, glyph->u1, glyph->v1, ...);
        }
        penX += glyph->advance;
    }
}
```

### 7.2 Glyph Atlas Interaction

- Atlas uses shelf packing algorithm for glyph placement
- Glyphs are cached by `(codepoint, fontSize, style, mono)` key
- Single-channel alpha texture (GL_ALPHA) for memory efficiency
- On-demand rasterization via FreeType when cache misses

### 7.3 Blend State Management

Blending is enabled during flush and disabled afterward for immediate mode compatibility:

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glDrawArrays(GL_TRIANGLES, 0, vertices.size());
// ... cleanup ...
glDisable(GL_BLEND);
```

### 7.4 Cross-Platform Considerations

- **Desktop (macOS/Linux):** GLSL 1.20 with `#version 120`
- **Web (Emscripten/WebGL):** No version directive, `precision mediump float` required
- Shaders use `attribute`/`varying` (GLSL 1.20) instead of `in`/`out` (GLSL 1.30+)

### 7.5 Buffer Strategy

- Single VBO shared across all render modes
- Pre-allocated with `GL_DYNAMIC_DRAW` for streaming
- `glBufferSubData` for partial updates (avoids full reallocation)
- Vector pre-reserved to `MAX_VERTICES` to avoid reallocations during frame

### 7.6 Attribute Binding

Attribute locations are explicitly bound before linking to ensure consistent layout:

```cpp
glBindAttribLocation(tprog, 0, "a_position");
glBindAttribLocation(tprog, 1, "a_texcoord");
glBindAttribLocation(tprog, 2, "a_color");
glLinkProgram(tprog);
```

This avoids runtime `glGetAttribLocation` queries and ensures all shader programs share the same vertex format.

---

## Summary

The `BatchRenderer` provides an efficient 2D rendering layer that:
- Minimizes draw calls through geometry batching
- Supports three rendering modes: solid color, textured glyphs, and full-color images
- Handles cross-platform shader compilation (desktop OpenGL / WebGL)
- Integrates with the glyph atlas for high-quality text rendering
- Maintains clean separation from the rasterizer's paint operation abstraction
