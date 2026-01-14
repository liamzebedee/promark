#include "freetype_font_provider.h"

FreeTypeFontProvider::FreeTypeFontProvider(FT_Face regularFace, FT_Face monoFace)
    : regularFace(regularFace), monoFace(monoFace) {
}

float FreeTypeFontProvider::getGlyphAdvance(uint32_t codepoint, float fontSize, bool monospace) const {
    FT_Face face = monospace ? monoFace : regularFace;

    // Fallback if requested face is not available
    if (!face) {
        face = regularFace ? regularFace : monoFace;
    }

    if (!face) {
        // No font available, use fallback width
        return getFallbackCharWidth(fontSize, monospace);
    }

    // Set font size
    FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(fontSize));

    // Get glyph index
    FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));

    // Load glyph to get metrics
    if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT) != 0) {
        return getFallbackCharWidth(fontSize, monospace);
    }

    // Return advance width (convert from 26.6 fixed point to float)
    return face->glyph->advance.x / 64.0f;
}

float FreeTypeFontProvider::getLineHeight(float fontSize, bool monospace) const {
    (void)monospace;
    // Line height equals font size (Typography::LINE_HEIGHT_RATIO = 1.0)
    return fontSize;
}
