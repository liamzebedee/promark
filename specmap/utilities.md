# Promark Utility Headers Specification

This document provides a comprehensive specification of the utility headers used in the Promark markdown editor engine.

## Overview

The Promark editor relies on three core utility headers that provide foundational functionality:

| Header | Purpose |
|--------|---------|
| `typography.h` | Defines typographic constants and font size calculations |
| `utf8.h` | Provides UTF-8 string handling and Unicode code point operations |
| `gl_includes.h` | Manages cross-platform OpenGL include directives |

All headers are located in `/protos/a/src/engine/` and use the `#pragma once` include guard pattern.

---

## Typography Header

**File:** `typography.h`

### Purpose

Provides a centralized typographic profile for the editor, defining font sizes, spacing values, and layout constants. All measurements are in pixels, calibrated for 2x DPI (Retina) displays.

### Namespace

All typography constants and functions reside in the `Typography` namespace.

### Font Size System

The header implements a **power-law font size distribution** using the "minor third" musical scale ratio (1.2).

#### Base Configuration

| Constant | Value | Description |
|----------|-------|-------------|
| `BASE_FONT_SIZE` | 28.0f | Body text size in pixels |
| `SCALE_RATIO` | 1.2f | Minor third scale multiplier |

#### Heading Sizes

Font sizes follow the formula: `BASE_FONT_SIZE * (SCALE_RATIO ^ (6 - level))`

| Constant | Multiplier | Approximate Size | Heading Level |
|----------|------------|------------------|---------------|
| `H1_SIZE` | 2.488 | ~70px | H1 (largest) |
| `H2_SIZE` | 2.074 | ~58px | H2 |
| `H3_SIZE` | 1.728 | ~48px | H3 |
| `H4_SIZE` | 1.44 | ~40px | H4 |
| `H5_SIZE` | 1.2 | ~34px | H5 |
| `H6_SIZE` | 1.0 | 28px | H6 (smallest) |

#### Heading Size Function

```cpp
inline float headingSize(int level);
```

Returns the appropriate font size for heading levels 1-6. Invalid levels return `BASE_FONT_SIZE`.

### Spacing Constants

#### Paragraph and Heading Margins

| Constant | Value | Description |
|----------|-------|-------------|
| `PARAGRAPH_MARGIN` | 14.0f | Space below paragraphs (0.5rem equivalent) |
| `HEADING_MARGIN_TOP` | 28.0f | Extra space before headings (1rem) |
| `HEADING_MARGIN_BOTTOM` | 14.0f | Space after headings (0.5rem) |
| `BLOCK_SPACING` | 14.0f | Uniform space between block elements |

#### Document Layout

| Constant | Value | Description |
|----------|-------|-------------|
| `DOCUMENT_MARGIN` | 50.0f | Margin around document content |

#### Blockquote Styling

| Constant | Value | Description |
|----------|-------|-------------|
| `BLOCKQUOTE_INDENT` | 20.0f | Left indent for blockquotes |
| `BLOCKQUOTE_BAR_WIDTH` | 3.0f | Width of the left border bar |

#### Code Blocks

| Constant | Value | Description |
|----------|-------|-------------|
| `CODE_BLOCK_PADDING` | 8.0f | Internal padding for code blocks |

#### Lists

| Constant | Value | Description |
|----------|-------|-------------|
| `LIST_INDENT` | 24.0f | Indentation per nesting level |

#### Line Height

| Constant | Value | Description |
|----------|-------|-------------|
| `LINE_HEIGHT_RATIO` | 1.0f | Multiplier (1.0 = line height equals font size) |

---

## UTF-8 Header

**File:** `utf8.h`

### Purpose

Provides utilities for handling UTF-8 encoded strings, including decoding to Unicode code points, string length calculation, and substring extraction by character index rather than byte offset.

### Namespace

All UTF-8 utilities reside in the `utf8` namespace.

### Dependencies

```cpp
#include <string>
#include <vector>
#include <cstdint>
```

### Functions

#### `decode` (Single Character)

```cpp
inline uint32_t decode(const std::string& str, size_t& pos);
```

Decodes a single UTF-8 character starting at the given position.

**Parameters:**
- `str` - The UTF-8 encoded string
- `pos` - Reference to current byte position (modified in-place)

**Returns:** Unicode code point (uint32_t)

**Behavior:**
- Advances `pos` by the number of bytes consumed
- Handles 1-byte ASCII (0xxxxxxx)
- Handles 2-byte sequences (110xxxxx 10xxxxxx)
- Handles 3-byte sequences (1110xxxx 10xxxxxx 10xxxxxx)
- Handles 4-byte sequences (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
- Invalid sequences treated as single bytes

#### `decode` (Full String)

```cpp
inline std::vector<uint32_t> decode(const std::string& str);
```

Decodes an entire UTF-8 string to a vector of Unicode code points.

**Parameters:**
- `str` - The UTF-8 encoded string

**Returns:** Vector of code points

**Performance Note:** Pre-reserves vector capacity equal to string byte length (upper bound on code point count).

#### `length`

```cpp
inline size_t length(const std::string& str);
```

Returns the number of Unicode code points (characters) in a UTF-8 string.

**Note:** This differs from `std::string::length()` which returns byte count.

#### `byteOffset`

```cpp
inline size_t byteOffset(const std::string& str, size_t charIndex);
```

Converts a character index to a byte offset.

**Parameters:**
- `str` - The UTF-8 encoded string
- `charIndex` - Zero-based character (code point) index

**Returns:** Byte offset corresponding to the character index

#### `substr`

```cpp
inline std::string substr(const std::string& str, size_t charStart, size_t charCount);
```

Extracts a substring using character indices rather than byte offsets.

**Parameters:**
- `str` - The UTF-8 encoded string
- `charStart` - Starting character index
- `charCount` - Number of characters to extract

**Returns:** Extracted substring as UTF-8 encoded string

**Boundary Handling:**
- Returns empty string if `charStart` exceeds string length
- Clamps `charCount` to available characters

### UTF-8 Encoding Reference

The implementation handles the standard UTF-8 byte patterns:

| Bytes | Code Point Range | First Byte | Continuation |
|-------|------------------|------------|--------------|
| 1 | U+0000 - U+007F | 0xxxxxxx | - |
| 2 | U+0080 - U+07FF | 110xxxxx | 10xxxxxx |
| 3 | U+0800 - U+FFFF | 1110xxxx | 10xxxxxx x2 |
| 4 | U+10000 - U+10FFFF | 11110xxx | 10xxxxxx x3 |

---

## OpenGL Includes Header

**File:** `gl_includes.h`

### Purpose

Provides a unified OpenGL include mechanism that resolves to the appropriate platform-specific headers. This enables the editor to compile for multiple targets (desktop, web) with a single codebase.

### Platform Detection

The header uses preprocessor conditionals to detect the build target:

```cpp
#ifdef __EMSCRIPTEN__
    // WebAssembly / Emscripten build
#elif defined(__APPLE__)
    // macOS build
#else
    // Linux / other desktop platforms
#endif
```

### Platform-Specific Includes

#### Emscripten (WebAssembly)

```cpp
#include <GLES2/gl2.h>
```

Uses OpenGL ES 2.0, the standard for WebGL.

#### macOS

```cpp
#include <OpenGL/gl.h>
```

Uses Apple's desktop OpenGL framework.

#### Linux / Other

```cpp
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
```

Uses desktop OpenGL with extensions. The `GL_GLEXT_PROTOTYPES` macro ensures that extension function prototypes are declared, avoiding the need for manual function pointer loading.

### Design Notes

- The editor targets **OpenGL ES 2.0 compatible** functionality across all platforms
- This provides maximum portability while maintaining sufficient rendering capabilities
- The header acts as an abstraction layer, allowing rendering code to use a common OpenGL API subset

---

## Implementation Notes

### Header Design Patterns

1. **Header-Only Implementation**: All three headers are header-only, using `inline` functions to avoid multiple definition errors.

2. **Namespace Encapsulation**: `Typography` and `utf8` namespaces prevent naming collisions with other code.

3. **Constexpr Constants**: Typography uses `constexpr` for compile-time constant evaluation.

4. **Cross-Platform Compatibility**: The `gl_includes.h` pattern enables single-source multi-platform builds.

### Performance Considerations

- UTF-8 decoding is O(n) for string operations since UTF-8 is a variable-width encoding
- `utf8::length()` and `utf8::byteOffset()` require scanning the string from the beginning
- Typography constants are compile-time, incurring no runtime overhead

### Integration Points

These utilities integrate with other engine components:

| Utility | Used By |
|---------|---------|
| Typography | Layout engine, text rendering |
| UTF-8 | Text input, cursor positioning, text selection |
| GL Includes | Rasterizer, batch renderer, glyph atlas |
