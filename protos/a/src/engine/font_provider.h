#pragma once
#include <cstdint>
#include <string>

// FontProvider: Abstract interface for font metrics
// This abstraction removes FreeType (FT_Face) from public APIs,
// enabling the layout layer to measure text without depending on
// platform-specific font implementations.
//
// The layout layer uses FontProvider for text measurement.
// The rendering layer (Rasterizer, GlyphAtlas) can use the concrete
// implementation or raw FT_Face internally.
//
// Spec reference: specs/02-layout-system.md - "No Platform Types in Public APIs"

class FontProvider {
public:
    virtual ~FontProvider() = default;

    // Get the advance width for a single glyph (how far cursor moves after this glyph)
    // fontSize: Font size in pixels
    // monospace: true for monospace/code font, false for proportional font
    // Returns: advance width in pixels
    virtual float getGlyphAdvance(uint32_t codepoint, float fontSize, bool monospace) const = 0;

    // Get line height for given font size
    // For now, line height equals font size (Typography::LINE_HEIGHT_RATIO = 1.0)
    virtual float getLineHeight(float fontSize, bool monospace) const = 0;

    // Get fallback character width estimate (for when glyph is missing)
    // Default implementation returns fontSize * 0.6
    virtual float getFallbackCharWidth(float fontSize, bool monospace) const {
        (void)monospace;
        return fontSize * 0.6f;
    }
};
