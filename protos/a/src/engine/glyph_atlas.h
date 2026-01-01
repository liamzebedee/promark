#pragma once
#include <cstdint>
#include <vector>
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "markdown_objects.h"

struct AtlasGlyph {
    float u0, v0, u1, v1;  // UV coordinates (normalized 0-1)
    int width, height;      // Glyph bitmap size
    int bearingX, bearingY; // Offset from baseline
    int advance;            // Horizontal advance
};

struct AtlasKey {
    uint32_t codepoint;
    int fontSize;
    uint8_t style;
    bool mono;

    bool operator<(const AtlasKey& o) const {
        if (codepoint != o.codepoint) return codepoint < o.codepoint;
        if (fontSize != o.fontSize) return fontSize < o.fontSize;
        if (style != o.style) return style < o.style;
        return mono < o.mono;
    }
};

class GlyphAtlas {
public:
    GlyphAtlas(int width = 1024, int height = 1024);
    ~GlyphAtlas();

    bool init();
    const AtlasGlyph* get(uint32_t codepoint, int fontSize, TextStyle style, bool mono, FT_Face face);
    void bind();
    unsigned int textureId() const { return texId; }

private:
    bool addToAtlas(int w, int h, const uint8_t* bitmap, float& u0, float& v0, float& u1, float& v1);

    int atlasW, atlasH;
    unsigned int texId;
    bool initialized;

    // Shelf packing
    int shelfY;      // Current shelf Y
    int shelfH;      // Current shelf height
    int shelfX;      // Next X in current shelf
    static constexpr int PAD = 1;

    std::map<AtlasKey, AtlasGlyph> cache;
};
