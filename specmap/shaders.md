# Promark Shader System Specification

## 1. Purpose and Overview

The Promark markdown editor uses a custom OpenGL-based rendering pipeline for drawing text, solid shapes (rectangles, carets, selections), and images. The shader system provides the GPU programs necessary to render these primitives efficiently.

The shader architecture is designed with the following goals:
- **Simplicity**: Minimal shaders that handle specific rendering tasks
- **Platform compatibility**: Works on both desktop (OpenGL 2.1/GLSL 1.20) and web (WebGL/ES 2.0)
- **Batched rendering**: Supports efficient vertex batching to minimize draw calls

## 2. Shader Types and Their Purposes

The system defines three shader programs:

| Shader | Purpose | Vertex Shader | Fragment Shader |
|--------|---------|---------------|-----------------|
| **Text** | Renders text glyphs from a texture atlas | `text.vert` | `text.frag` |
| **Solid** | Renders solid-colored rectangles (backgrounds, carets, selections) | `solid.vert` | `solid.frag` |
| **Image** | Renders images with full RGBA texture sampling | `text.vert` (reused) | `image.frag` |

### 2.1 Text Shader

Used for rendering text characters from the glyph atlas. The atlas stores grayscale alpha values representing glyph coverage, which are combined with a per-vertex color to produce anti-aliased text.

### 2.2 Solid Shader

Used for rendering solid-colored geometry such as:
- Text selection highlights
- Caret/cursor
- Background fills
- Horizontal rules and other decorations

### 2.3 Image Shader

Used for rendering embedded images in the markdown document. Unlike the text shader, it samples the full RGBA color from the texture rather than just the alpha channel.

## 3. Vertex Shader Inputs/Outputs

### 3.1 Text Vertex Shader (`text.vert`)

**Attributes (inputs):**

| Name | Type | Location | Description |
|------|------|----------|-------------|
| `a_position` | `vec2` | 0 | Screen-space position (x, y) |
| `a_texcoord` | `vec2` | 1 | Texture coordinates (u, v) into glyph atlas |
| `a_color` | `vec4` | 2 | Per-vertex RGBA color |

**Uniforms:**

| Name | Type | Description |
|------|------|-------------|
| `u_projection` | `mat4` | Orthographic projection matrix |

**Varyings (outputs to fragment shader):**

| Name | Type | Description |
|------|------|-------------|
| `v_texcoord` | `vec2` | Interpolated texture coordinates |
| `v_color` | `vec4` | Interpolated vertex color |

**Transformation:**
```glsl
gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
```
The 2D position is expanded to a 4D homogeneous coordinate with z=0, then transformed by the projection matrix.

### 3.2 Solid Vertex Shader (`solid.vert`)

**Attributes (inputs):**

| Name | Type | Location | Description |
|------|------|----------|-------------|
| `a_position` | `vec2` | 0 | Screen-space position (x, y) |
| `a_color` | `vec4` | 2 | Per-vertex RGBA color |

Note: The solid shader does not use `a_texcoord` (location 1) since it renders untextured geometry.

**Uniforms:**

| Name | Type | Description |
|------|------|-------------|
| `u_projection` | `mat4` | Orthographic projection matrix |

**Varyings (outputs to fragment shader):**

| Name | Type | Description |
|------|------|-------------|
| `v_color` | `vec4` | Interpolated vertex color |

## 4. Fragment Shader Logic

### 4.1 Text Fragment Shader (`text.frag`)

```glsl
void main() {
    float alpha = texture2D(u_texture, v_texcoord).a;
    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
```

**Logic:**
1. Sample the alpha channel from the glyph atlas texture at `v_texcoord`
2. Use the interpolated vertex color RGB values directly
3. Multiply the vertex alpha by the texture alpha for the final alpha value

This approach allows:
- Single-channel (alpha-only) glyph atlas storage
- Per-glyph coloring via vertex attributes
- Proper alpha blending for anti-aliased text edges

### 4.2 Solid Fragment Shader (`solid.frag`)

```glsl
void main() {
    gl_FragColor = v_color;
}
```

**Logic:**
Simply outputs the interpolated vertex color. The simplest possible fragment shader for solid-colored geometry.

### 4.3 Image Fragment Shader (`image.frag`)

```glsl
void main() {
    vec4 texColor = texture2D(u_texture, v_texcoord);
    gl_FragColor = texColor * v_color;
}
```

**Logic:**
1. Sample full RGBA color from the image texture
2. Multiply by the vertex color (component-wise)

The vertex color multiplication allows for:
- Tinting images
- Alpha modulation for fade effects
- Default white vertex color (1,1,1,1) for unmodified display

## 5. Uniform Variables Summary

| Uniform | Type | Used By | Description |
|---------|------|---------|-------------|
| `u_projection` | `mat4` | All shaders | Orthographic projection matrix (top-left origin) |
| `u_texture` | `sampler2D` | Text, Image | Texture unit 0 binding |

### 5.1 Projection Matrix Setup

The projection matrix is an orthographic projection configured for 2D rendering:

```cpp
// Orthographic projection (top-left origin)
float l = 0, r = (float)width, t = 0, b = (float)height;
float n = -1, f = 1;

projMatrix[0] = 2.0f / (r - l);
projMatrix[5] = 2.0f / (t - b);  // Flip Y for top-left origin
projMatrix[10] = -2.0f / (f - n);
projMatrix[12] = -(r + l) / (r - l);
projMatrix[13] = -(t + b) / (t - b);
projMatrix[14] = -(f + n) / (f - n);
projMatrix[15] = 1.0f;
```

This creates a coordinate system where:
- Origin (0,0) is at the top-left
- Y increases downward (standard screen coordinates)
- Pixel coordinates map directly to screen positions

Scroll offset is applied by modifying the Y translation component:
```cpp
projMatrix[13] += scrollOffsetY * projMatrix[5];
```

## 6. Shader Embedding System

### 6.1 File Organization

```
src/engine/
    shaders/
        text.vert     # Text vertex shader source
        text.frag     # Text fragment shader source
        solid.vert    # Solid vertex shader source
        solid.frag    # Solid fragment shader source
        image.frag    # Image fragment shader source
    shaders_embedded.h  # Auto-generated header with embedded sources
```

### 6.2 Embedding Format

Shader sources are embedded as C++ raw string literals in `shaders_embedded.h`:

```cpp
namespace Shaders {

const char* TEXT_VERT = R"(
attribute vec2 a_position;
attribute vec2 a_texcoord;
attribute vec4 a_color;
// ... rest of shader
)";

const char* TEXT_FRAG = R"(
varying vec2 v_texcoord;
// ... rest of shader
)";

// Additional shaders...

} // namespace Shaders
```

The header includes a warning comment:
```cpp
// Auto-generated shader sources - do not edit manually
// Generated from src/engine/shaders/*.vert and *.frag
```

### 6.3 Platform-Specific Preambles

At runtime, platform-specific version directives are prepended to the shader sources:

**Desktop (OpenGL 2.1):**
```cpp
const char* vertPreamble = "#version 120\n";
const char* fragPreamble = "#version 120\n";
```

**Web (WebGL/Emscripten):**
```cpp
const char* vertPreamble = "";
const char* fragPreamble = "precision mediump float;\n";
```

This allows the same shader code to run on both desktop OpenGL and WebGL.

## 7. Notable Implementation Details

### 7.1 Attribute Location Binding

Attribute locations are explicitly bound before program linking to ensure consistent vertex attribute indices:

```cpp
glBindAttribLocation(tprog, 0, "a_position");
glBindAttribLocation(tprog, 1, "a_texcoord");
glBindAttribLocation(tprog, 2, "a_color");
```

This allows the BatchRenderer to use fixed attribute indices without querying locations.

### 7.2 Image Shader Reuses Text Vertex Shader

The image shader program reuses `TEXT_VERT` as its vertex shader:

```cpp
glShaderSource(ivs, 1, &textVSrc, nullptr);  // Reuse text vertex shader
```

This is possible because both text and image rendering need the same vertex layout (position, texcoord, color). Only the fragment processing differs.

### 7.3 Vertex Structure

All shaders expect vertices with the following structure:

```cpp
struct Vertex {
    float x, y;     // Position
    float u, v;     // Texture coordinates
    float r, g, b, a;  // Color
};
```

Total size: 32 bytes per vertex (8 floats).

### 7.4 Blending Configuration

All rendering uses standard alpha blending:

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

This enables proper transparency for:
- Anti-aliased text edges
- Semi-transparent selections
- Image alpha channels

### 7.5 Batch Rendering and Shader Switching

The BatchRenderer tracks whether it's currently rendering textured or solid geometry:

- Switching from solid to textured (or vice versa) triggers a flush
- Image rendering always flushes first and uses its own draw call
- This minimizes shader program switches during typical rendering

### 7.6 GLSL Version Compatibility

The shaders use GLSL 1.20 / ES 2.0 compatible syntax:
- `attribute` instead of `in` for vertex inputs
- `varying` instead of `in`/`out` for shader inter-stage variables
- `texture2D()` instead of `texture()`
- `gl_FragColor` instead of output variable

This ensures compatibility with older OpenGL implementations and WebGL 1.0.
