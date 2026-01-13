# Shaders Design Analysis

**Files:**
- `src/engine/shaders/text.vert`
- `src/engine/shaders/text.frag`
- `src/engine/shaders/solid.vert`
- `src/engine/shaders/solid.frag`
- `src/engine/shaders/image.frag`

**Related:**
- `src/engine/shaders_embedded.h` (auto-generated embedding)
- `src/engine/batch_renderer.cpp` (consumer)

---

## 1. Responsibilities

### text.vert (lines 1-14)
Transforms 2D textured quads for glyph rendering.

1. **Vertex transformation** - Projects 2D position to clip space via orthographic matrix (line 11)
2. **Texcoord passthrough** - Forwards UV coordinates for glyph atlas lookup (line 12)
3. **Color passthrough** - Forwards per-vertex RGBA for tinting (line 13)

### text.frag (lines 1-9)
Renders alpha-only glyphs from a glyph atlas texture.

1. **Alpha extraction** - Samples only the alpha channel from texture (line 7)
2. **Color composition** - Uses vertex color RGB with modulated alpha (line 8)

This is the correct approach for single-channel glyph atlases where glyphs are stored as alpha masks.

### solid.vert (lines 1-11)
Transforms 2D colored quads without texture coordinates.

1. **Vertex transformation** - Projects 2D position to clip space (line 9)
2. **Color passthrough** - Forwards per-vertex RGBA (line 10)

Note: Does **not** declare `a_texcoord` attribute, unlike text.vert.

### solid.frag (lines 1-5)
Outputs solid colors without texture sampling.

1. **Direct output** - Passes through interpolated vertex color unchanged (line 4)

### image.frag (lines 1-9)
Renders full-color textures (images) with tinting support.

1. **Texture sampling** - Samples full RGBA from texture (line 7)
2. **Color modulation** - Multiplies texture by vertex color for tinting (line 8)

Key difference from text.frag: Uses all four texture channels, not just alpha.

---

## 2. Dependencies

### Uniform Dependencies

| Shader | Uniform | Type | Purpose |
|--------|---------|------|---------|
| text.vert | `u_projection` | mat4 | Orthographic projection matrix |
| solid.vert | `u_projection` | mat4 | Orthographic projection matrix |
| text.frag | `u_texture` | sampler2D | Glyph atlas (alpha-only) |
| image.frag | `u_texture` | sampler2D | Image texture (RGBA) |

All vertex shaders share the same projection uniform name. This enables the BatchRenderer to use a single uniform lookup pattern (batch_renderer.cpp line 152).

### Attribute Dependencies

| Shader | Attributes | Vertex Layout |
|--------|------------|---------------|
| text.vert | `a_position`, `a_texcoord`, `a_color` | Full Vertex struct |
| solid.vert | `a_position`, `a_color` | Position + color only |
| image.frag | (none - uses text.vert) | Full Vertex struct |

**Critical binding locations** (from batch_renderer.cpp lines 52-54, 71-72, 92-94):
- Location 0: `a_position`
- Location 1: `a_texcoord`
- Location 2: `a_color`

### Varying Dependencies

| Varying | text.vert/frag | solid.vert/frag | image.frag |
|---------|----------------|-----------------|------------|
| `v_texcoord` | Yes | No | Yes (from text.vert) |
| `v_color` | Yes | Yes | Yes |

---

## 3. Mutation Points

Shaders produce the following outputs:

| Shader | Output | Description |
|--------|--------|-------------|
| text.vert | `gl_Position` | Clip-space position |
| text.vert | `v_texcoord`, `v_color` | Interpolated varyings |
| text.frag | `gl_FragColor` | Fragment color (alpha-masked) |
| solid.vert | `gl_Position` | Clip-space position |
| solid.vert | `v_color` | Interpolated color |
| solid.frag | `gl_FragColor` | Fragment color (solid) |
| image.frag | `gl_FragColor` | Fragment color (textured) |

**State assumptions:**
- All fragment shaders output to `gl_FragColor` (GLSL 1.20 / ES 2.0 style)
- Blending mode (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`) is set by BatchRenderer (line 173, 280), not controlled by shaders

---

## 4. Boundary Violations

### 4.1 Missing Vertex Shader for image.frag

**Location:** image.frag has no corresponding image.vert

The image shader reuses text.vert as its vertex shader (batch_renderer.cpp lines 83-84):

```cpp
// Image shader (reuses text.vert, different fragment shader)
glShaderSource(ivs, 1, &textVSrc, nullptr);  // Reuse text vertex shader
```

This is an implicit coupling. If text.vert changes its varying interface, image.frag could silently break. The dependency is:

```
image.frag -> text.vert (implicit, via runtime pairing)
```

**Concern:** The shader files on disk don't communicate this relationship. A developer modifying text.vert wouldn't know image.frag depends on it.

**Correct resolution:** Either:
1. Create explicit `image.vert` (even if identical to text.vert) for clarity
2. Rename to `textured.vert` to indicate shared use by text and image pipelines
3. Add a comment in image.frag noting the dependency

### 4.2 Attribute Layout Divergence

**Location:** solid.vert line 1-2 vs text.vert lines 1-3

solid.vert declares:
```glsl
attribute vec2 a_position;
attribute vec4 a_color;
```

text.vert declares:
```glsl
attribute vec2 a_position;
attribute vec2 a_texcoord;
attribute vec4 a_color;
```

The BatchRenderer binds solid shader attributes at locations 0 and 2, skipping location 1 (batch_renderer.cpp lines 71-72). This works because:
1. Location 1 (`a_texcoord`) is still enabled (line 164)
2. solid.frag never reads `v_texcoord`
3. GPU ignores the unused attribute

However, drawRect still populates texcoord data in the vertex buffer (batch_renderer.cpp lines 231-237):
```cpp
Vertex v[6] = {
    {x,     y,     0, 0, r, g, b, a},  // u=0, v=0 provided but unused
```

**Concern:** Wasted bandwidth sending texcoord data for solid geometry. The Vertex struct always includes u,v even when the shader ignores them.

### 4.3 Hardcoded Z Values

**Location:** text.vert line 11, solid.vert line 9

```glsl
gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
```

Both vertex shaders hardcode Z=0. This prevents:
- Depth-based layering
- Z-ordering without painter's algorithm
- Future 3D effects

The BatchRenderer projection matrix sets near=-1, far=1 (batch_renderer.cpp lines 121, 126), but the shaders cannot utilize this range.

---

## 5. Declared-but-Unrealised Design

### 5.1 image.frag v_color Tinting Never Used

**Location:** image.frag lines 7-8, batch_renderer.cpp lines 258-265

```glsl
vec4 texColor = texture2D(u_texture, v_texcoord);
gl_FragColor = texColor * v_color;
```

The shader supports color tinting via multiplication with v_color. However, BatchRenderer::drawImage always passes white (1,1,1,1):

```cpp
Vertex v[6] = {
    {x,     y,     0, 0, 1, 1, 1, 1},
    {x + w, y,     1, 0, 1, 1, 1, 1},
    // ... all vertices use (1,1,1,1) for color
```

**Implication:** The tinting capability exists but is never exercised. The multiplication `texColor * v_color` is a no-op when v_color is white.

**Use cases implied but not implemented:**
- Image dimming (multiply by gray)
- Color overlays (tint toward a hue)
- Fade-in/fade-out (alpha < 1.0)

### 5.2 No Gamma Correction

**Location:** All fragment shaders

None of the shaders perform gamma correction. Colors are assumed to be in linear space, but:
- Input images are typically sRGB
- Display expects sRGB
- Text rendering without gamma correction produces incorrect alpha blending

This is a common issue but worth noting as the shader architecture provides no hooks for colorspace conversion.

### 5.3 No Screen-Space Effects

The shader suite is minimal, providing only:
- Solid color
- Alpha-masked text
- Textured quads

Absent capabilities that the structure could support:
- Outline/glow effects for text
- Drop shadows
- Rounded corners (would require SDF or procedural geometry)
- Blur/post-processing

This is appropriate for the current scope but the rigid fragment output pattern (`gl_FragColor = ...`) leaves no room for multi-pass effects.

### 5.4 GLSL Version Flexibility Not Shader-Native

**Location:** batch_renderer.cpp lines 22-29

```cpp
#ifdef __EMSCRIPTEN__
    const char* vertPreamble = "";
    const char* fragPreamble = "precision mediump float;\n";
#else
    const char* vertPreamble = "#version 120\n";
    const char* fragPreamble = "#version 120\n";
#endif
```

Version preambles are injected at runtime rather than declared in shader files. The raw .frag/.vert files have no version declaration, making them technically invalid GLSL that only works due to driver tolerance or the injected preamble.

**Concern:** The shader files cannot be validated standalone by external tools (glslangValidator, etc.) without preprocessing.

### 5.5 shaders_embedded.h Synchronization Risk

**Location:** src/engine/shaders_embedded.h lines 1-2

```cpp
// Auto-generated shader sources - do not edit manually
// Generated from src/engine/shaders/*.vert and *.frag
```

The header claims to be auto-generated, but there's no visible build step that regenerates it. If a developer modifies the .vert/.frag files directly, shaders_embedded.h could become stale.

**Observation:** The content of shaders_embedded.h exactly matches the source files, so they appear synchronized currently. But the generation mechanism is undocumented.

---

## Summary of Architectural Issues

| Issue | Severity | Remediation |
|-------|----------|-------------|
| image.frag implicitly depends on text.vert | Medium | Create image.vert or rename to textured.vert |
| Texcoord data sent for solid geometry | Low | Create SolidVertex struct without u,v, or accept waste |
| v_color tinting unused in drawImage | Low | Document as intentional or expose tint parameter |
| No gamma correction | Low | Accept limitation or add sRGB conversion pass |
| Hardcoded Z=0 prevents depth ordering | Low | Accept limitation or add z attribute |
| Shader files have no version declaration | Low | Add conditional version via preprocessor or accept runtime injection |
| shaders_embedded.h generation undocumented | Low | Document build step or add regeneration script |

---

## Shader Program Matrix

| Program | Vertex Shader | Fragment Shader | Use Case |
|---------|---------------|-----------------|----------|
| textProg | text.vert | text.frag | Glyph rendering from atlas |
| solidProg | solid.vert | solid.frag | Rectangles, lines, backgrounds |
| imageProg | text.vert | image.frag | Embedded images |

The reuse of text.vert for imageProg is the key architectural coupling that should be documented or resolved.
