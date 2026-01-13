# Layout System

## Purpose

The LayoutTree represents the geometric structure of the document. It answers the question: "Where is everything positioned?"

## Current Problems

Authority is split: four of eight layout object types fully manage their own positioning, while the engine coordinates others. Two positioning systems coexist (relative and absolute), coordinated by a `skipPropagate` flag. The `layoutInlineFlow` method is a stub. FreeType faces leak through public APIs across five layers. Image loading happens during layout.

The result: nobody knows who's responsible for positioning, platform details are everywhere, and I/O is disguised as geometry computation.

## Target Model

### Engine Coordinates, Objects Measure

The LayoutEngine is the sole coordinator of positioning. Layout objects only report their intrinsic size requirements - they never position themselves or their children.

This eliminates the split authority. There's one answer to "who positioned this element?": the LayoutEngine.

### Fully Resolved Styles

Each LayoutNode contains fully resolved style information: concrete font sizes, specific colors, exact spacing values. The layout phase copies and resolves everything it needs from the document tree.

This means the painter never queries back to find out "what kind of element is this?" - the layout node already contains the visual properties. A heading's large font size is stored directly, not derived from its semantic type.

Importantly, this includes **debug colors**. Each node's role (paragraph, heading, code block) maps to a debug visualization color at layout time. The painter just reads the color - no type-switching needed.

### Box Model

Each LayoutNode has a bounds rectangle (the content box) plus margin and padding values. Helper methods compute the padding box and margin box from these values.

This supports debug visualization of the full box model without any extra machinery.

### No Platform Types in APIs

FreeType faces don't appear in public interfaces. The LayoutEngine receives a FontProvider abstraction that can measure and shape text. The actual FreeType implementation is hidden behind this interface.

This enables testing layout without a real font system, and makes future font backend changes possible without touching layout code.

### No I/O in Layout

Image dimensions are loaded before layout begins by a separate ImageLoader service. The layout phase receives pre-loaded metadata (dimensions, identifiers) and uses it for positioning. Actual texture loading happens later in the rasterizer.

Geometry computation stays pure: given inputs, compute outputs. No file system access, no base64 decoding, no network requests.

## What the LayoutNode Contains

- **bounds**: Position and size of the content box
- **baseline**: For text alignment
- **style**: Fully resolved visual properties (fonts, colors, spacing, debug color)
- **content**: Type-specific data (text runs with glyphs, image metadata, table structure)
- **children**: Child layout nodes
- **source**: Byte range in original text (for hit-testing)
- **atomic**: Whether this element is indivisible (images, equations)

## Hit-Testing

The LayoutTree owns hit-testing. Given screen coordinates (adjusted for scroll), it returns a source position.

**Algorithm**:
1. Find the leaf node whose bounds contain the point
2. For text nodes: find the nearest character using glyph positions
3. For atomic nodes: return the whole element's source range
4. For empty space: find nearest content position

**Mode Switching**: When switching between visual and raw mode, the caret tracks the *character*, not the byte offset. If the caret is on "G" in visual mode, it stays on "G" in raw mode, even though the byte position changes due to visible syntax.

**Invisible Syntax**: In visual mode, positions inside hidden syntax (like `**`) are unreachable by clicking. Arrow keys skip over them. The caret only rests on positions that correspond to visible characters.

**Click Targets**:
- Decorative elements (bullets, blockquote bars): clicks ignored
- Links: click follows the link (edit via keyboard)
- Images: click selects entire image
- Empty space: click finds nearest content

See [06-edge-cases.md](06-edge-cases.md) for complete hit-testing rules.

## What Gets Deleted

- The InlineLayoutObject class
- The layoutInlineFlow stub method
- The LayoutFlow::Inline enum value
- The skipPropagate flag
- FT_Face from all public APIs
- Image loading code from ImageLayoutObject

## Success Criteria

The LayoutEngine is the single coordinator of all positioning. No platform types appear in public interfaces. No I/O happens during layout. Debug boxes can be drawn using only data already present in layout nodes.
