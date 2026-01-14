#pragma once
#include "font_provider.h"
#include <ft2build.h>
#include FT_FREETYPE_H

// FreeTypeFontProvider: Concrete FontProvider implementation using FreeType
// This class encapsulates FT_Face handles and provides font metrics to the
// layout layer without exposing FreeType types in public interfaces.
//
// Note: This class does NOT own the FT_Face handles - they are owned by
// the Rasterizer or Engine and must outlive this provider.

class FreeTypeFontProvider : public FontProvider {
public:
    // Create a font provider with the given font faces
    // regularFace: Font face for proportional text (can be nullptr for fallback)
    // monoFace: Font face for monospace/code text (can be nullptr for fallback)
    FreeTypeFontProvider(FT_Face regularFace, FT_Face monoFace);
    ~FreeTypeFontProvider() override = default;

    // FontProvider interface
    float getGlyphAdvance(uint32_t codepoint, float fontSize, bool monospace) const override;
    float getLineHeight(float fontSize, bool monospace) const override;

    // Access to underlying faces (for components that need them, e.g., rendering)
    // This is intentionally NOT part of the FontProvider interface
    FT_Face getRegularFace() const { return regularFace; }
    FT_Face getMonoFace() const { return monoFace; }

private:
    FT_Face regularFace;
    FT_Face monoFace;
};
