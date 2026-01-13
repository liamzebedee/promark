# Utilities Design Analysis

This document analyzes the header-only utility files that provide foundational support across the engine.

---

## 1. utf8.h

**Location:** `src/engine/utf8.h`

### Responsibilities

The file provides UTF-8 string decoding utilities within the `utf8` namespace:

1. **Single codepoint decoding** (lines 11-61): Decode one UTF-8 character from a string position, advancing the position reference
2. **Full string decoding** (lines 64-74): Convert an entire UTF-8 string to a vector of Unicode codepoints
3. **Length calculation** (lines 77-85): Count codepoints in a UTF-8 string
4. **Byte offset lookup** (lines 88-96): Find the byte position of the nth codepoint
5. **Substring extraction** (lines 99-105): Extract a substring using codepoint indices rather than byte indices

### Dependencies

- `<string>` - For `std::string` input handling
- `<vector>` - For returning codepoint arrays
- `<cstdint>` - For `uint8_t` and `uint32_t` types

**Why:** These are pure STL dependencies with no engine coupling, appropriate for a leaf utility.

### Mutation Points

| Mutation | Location | Authority |
|----------|----------|-----------|
| `pos` parameter | lines 12-59 | Caller provides reference, function advances it |

The only state mutation is the `pos` reference parameter in `decode(const std::string&, size_t&)`. This is an output parameter pattern - authority correctly resides with the caller who controls iteration.

**No global state.** All functions are pure or use explicit output parameters.

### Boundary Violations

**None identified.** This file is a pure utility with no engine dependencies. It sits correctly at the bottom of the dependency graph.

### Declared-but-Unrealised Design

1. **No encoding support** (line 6 comment: "UTF-8 utilities for decoding to Unicode code points")
   - The namespace is `utf8` but only decoding is implemented
   - No `encode()` function to convert codepoints back to UTF-8
   - **Implication:** Any component needing to produce UTF-8 output (clipboard, file save) must implement encoding elsewhere or use a different library

2. **Error handling is silent** (lines 42-46, 51-54)
   - Invalid UTF-8 sequences return the raw byte and advance by 1
   - No error reporting mechanism (callback, exception, return code)
   - **Workaround code likely exists** in callers that need to handle malformed input

3. **No BOM handling**
   - UTF-8 files may start with a BOM (`0xEF 0xBB 0xBF`)
   - No utility to detect or skip it
   - **Implication:** File loading code must handle BOM separately

---

## 2. gl_includes.h

**Location:** `src/engine/gl_includes.h`

### Responsibilities

Provide a single include point for OpenGL headers across platforms:

1. **Emscripten/WebGL** (lines 4-5): Include GLES2 headers
2. **macOS** (lines 6-7): Include Apple's OpenGL headers
3. **Linux** (lines 8-12): Include desktop GL with extension prototypes

### Dependencies

- `<GLES2/gl2.h>` - Emscripten
- `<OpenGL/gl.h>` - macOS
- `<GL/gl.h>`, `<GL/glext.h>` - Linux

**Why:** Platform abstraction for OpenGL. All rendering code should include this instead of raw GL headers.

### Mutation Points

**None.** This file only contains preprocessor directives and includes. No state exists.

### Boundary Violations

**None identified.** This is a pure platform abstraction layer sitting at the bottom of the graphics stack.

### Declared-but-Unrealised Design

1. **"OpenGL ES 2.0 style" claim** (line 3 comment)
   - The comment promises ES 2.0 compatibility
   - macOS path (line 7) includes `<OpenGL/gl.h>` which is desktop GL, not GLES2
   - **Asymmetry:** Emscripten gets true GLES2, but macOS/Linux get desktop GL
   - **Implication:** Code using this header must avoid desktop-GL-only features, but no compile-time enforcement exists

2. **No Windows support**
   - The `#else` branch (line 8) assumes Linux
   - Windows would fall into the Linux path incorrectly
   - **Missing:** `#elif defined(_WIN32)` branch with appropriate includes (likely `<GL/glew.h>` or similar)

3. **No version/capability checking**
   - Header selection is compile-time only
   - No runtime capability queries exposed
   - **Implication:** Code must assume minimum common denominator or implement its own capability detection

4. **`GL_GLEXT_PROTOTYPES` macro** (line 10)
   - This macro is defined only for the Linux path
   - It forces static linking to extension functions
   - **Alternative design:** Dynamic loading via `glXGetProcAddress` would be more robust but is not provided

---

## 3. shaders_embedded.h

**Location:** `src/engine/shaders_embedded.h`

### Responsibilities

Embed GLSL shader source code as string literals within the `Shaders` namespace:

1. **TEXT_VERT / TEXT_FRAG** (lines 7-34): Text rendering shaders (glyph atlas sampling)
2. **SOLID_VERT / SOLID_FRAG** (lines 36-56): Solid color rendering (backgrounds, UI)
3. **IMAGE_FRAG** (lines 58-68): Image texture rendering

### Dependencies

**None.** Pure string literals with no includes required.

### Mutation Points

**None.** All data is `const char*` compile-time constants.

### Boundary Violations

**None identified.** This file is data-only and has no behavioral dependencies.

### Declared-but-Unrealised Design

1. **Auto-generated claim** (line 1: "Auto-generated shader sources - do not edit manually")
   - Comment claims generation from `src/engine/shaders/*.vert` and `*.frag`
   - **If true:** A build step should regenerate this file, but that pipeline is not evident in the file itself
   - **If false:** The comment is misleading and should be removed
   - **Risk:** Manual edits to embedded shaders will be lost if regeneration occurs

2. **Missing IMAGE_VERT shader**
   - `IMAGE_FRAG` exists (lines 58-68) but no `IMAGE_VERT`
   - **Implication:** Image rendering reuses `TEXT_VERT` (which has `a_texcoord`)
   - **Asymmetry:** The naming suggests a complete IMAGE shader pair should exist
   - **Workaround:** Calling code must know to pair `TEXT_VERT` with `IMAGE_FRAG`

3. **No precision qualifiers**
   - GLSL ES requires precision qualifiers (`precision mediump float;`)
   - Desktop GL ignores them but doesn't require them
   - **Risk:** These shaders may fail on strict GLES2 implementations
   - **Workaround:** The loader code must prepend precision qualifiers, or they're already in a wrapper

4. **No version directive**
   - Shaders lack `#version` directives
   - Relies on implementation default (typically GLSL 1.10 or ESSL 1.00)
   - **Implication:** Cannot use newer GLSL features; locked to legacy syntax

5. **Hardcoded attribute names**
   - `a_position`, `a_texcoord`, `a_color` are embedded in shaders
   - **Coupling:** Vertex buffer layout code must match these exact names
   - **Missing:** No shared constants defining attribute locations or names

---

## 4. typography.h

**Location:** `src/engine/typography.h`

### Responsibilities

Define typographic constants for document layout within the `Typography` namespace:

1. **Font size scale** (lines 9-23): Base size and heading hierarchy using minor third ratio
2. **Heading size lookup** (lines 26-36): Function to get size by level number
3. **Spacing constants** (lines 38-57): Margins, padding, and indentation values
4. **Line height** (line 60): Line height multiplier

### Dependencies

**None.** Pure `constexpr` values with no includes required.

### Mutation Points

**None.** All values are compile-time constants.

### Boundary Violations

**None identified.** This file is a configuration/constants layer with no behavioral code.

### Declared-but-Unrealised Design

1. **Power-law claim vs. hardcoded values** (lines 3-4, 16-23)
   - Comment describes a power-law relationship: `BASE * (SCALE_RATIO ^ (6 - level))`
   - Constants are hardcoded approximations, not computed
   - **Inconsistency:** `SCALE_RATIO` (line 14) is defined but not used in the constants
   - **Correct values vs. defined:**
     - H1 should be `16 * 1.2^5 = 39.81`, defined as `16 * 2.488 = 39.81` - matches
     - H2 should be `16 * 1.2^4 = 33.18`, defined as `16 * 2.074 = 33.18` - matches
   - **Design gap:** If `SCALE_RATIO` changes, all `H*_SIZE` constants must be manually updated
   - **Missing:** `constexpr` function to compute sizes from ratio, or use of `pow()` (though `pow` isn't constexpr)

2. **`headingSize()` function duplicates constants** (lines 26-36)
   - Switch statement manually maps levels to constants
   - **Alternative:** Could compute `BASE_FONT_SIZE * pow(SCALE_RATIO, 6 - level)` at runtime
   - **Risk:** If constants are updated, switch must also be updated

3. **LINE_HEIGHT_RATIO = 1.0** (line 60)
   - Setting line height equal to font size means no inter-line spacing
   - Typical values are 1.2-1.5 for readability
   - **Either:** This is intentional (tight layout) or a placeholder awaiting tuning
   - **Implication:** Text lines will touch vertically if this is used directly

4. **No font weight/style constants**
   - Only sizes are defined
   - **Missing:** Font weight for headings vs. body, italic style for emphasis
   - **Implication:** Font selection logic must hardcode weight/style elsewhere

5. **Pixel units only** (line 4: "All sizes in pixels")
   - No DPI awareness or scaling factors
   - **Implication:** High-DPI displays will render text at incorrect physical sizes
   - **Missing:** `SCALE_FACTOR` or function to convert logical to physical pixels

6. **Spacing constants lack hierarchy**
   - `PARAGRAPH_MARGIN`, `HEADING_MARGIN_TOP`, etc. are flat constants
   - No relationship to `BASE_FONT_SIZE` or `SCALE_RATIO`
   - **Inconsistency:** Comment on line 39 says "0.5rem equivalent" but `rem` implies relative sizing
   - **Workaround:** If base font size changes, margins won't scale proportionally

---

## Cross-Cutting Concerns

### Layering Summary

```
                    [Application Code]
                           |
                           v
              +------------------------+
              |   shaders_embedded.h   |  (data)
              |   typography.h         |  (config)
              +------------------------+
                           |
                           v
              +------------------------+
              |   utf8.h               |  (string utility)
              +------------------------+
                           |
                           v
              +------------------------+
              |   gl_includes.h        |  (platform abstraction)
              +------------------------+
                           |
                           v
                    [System Headers]
```

All four files correctly occupy utility/leaf positions with no upward dependencies.

### Cohesion Analysis

| File | Cohesion | Notes |
|------|----------|-------|
| `utf8.h` | High | Single responsibility: UTF-8 decoding |
| `gl_includes.h` | High | Single responsibility: Platform GL abstraction |
| `shaders_embedded.h` | High | Single responsibility: Shader source storage |
| `typography.h` | Medium | Mixes font sizes, spacing, and document margins |

`typography.h` could potentially be split into:
- `font_scale.h` - Font size calculations
- `spacing.h` - Layout spacing constants
- `document_layout.h` - Document-level margins

### Missing Utilities

Based on these files, the following utilities are implied but not present:

1. **UTF-8 encoder** - Needed for any text output
2. **DPI/scale factor utilities** - Needed for `typography.h` to work on high-DPI displays
3. **Shader compilation utilities** - Something must compile the strings in `shaders_embedded.h`
4. **Attribute location constants** - Shared between shaders and vertex buffer setup

### Header-Only Trade-offs

All files are header-only with `inline` functions or `constexpr` values:

**Benefits:**
- No separate compilation unit needed
- Easy to include and use
- Compile-time optimization opportunities

**Risks:**
- Multiple inclusion in different translation units increases compile time
- `inline` functions are duplicated in each TU (linker deduplication helps)
- No separate testing without including in a test TU

---

## Summary of Architectural Issues

| File | Issue | Severity | Impact |
|------|-------|----------|--------|
| `utf8.h` | No encode function | Medium | Limits text output capabilities |
| `utf8.h` | Silent error handling | Low | May hide malformed input |
| `gl_includes.h` | macOS uses desktop GL, not ES 2.0 | Low | May allow non-portable code |
| `gl_includes.h` | No Windows support | Medium | Blocks Windows port |
| `shaders_embedded.h` | Missing IMAGE_VERT | Low | Implicit pairing with TEXT_VERT |
| `shaders_embedded.h` | No precision qualifiers | Medium | May fail on strict GLES2 |
| `shaders_embedded.h` | Auto-generated claim unclear | Low | Risk of lost edits |
| `typography.h` | LINE_HEIGHT_RATIO = 1.0 | Medium | Poor default readability |
| `typography.h` | No DPI awareness | High | Incorrect sizing on high-DPI |
| `typography.h` | Hardcoded power-law values | Low | Maintenance burden |
