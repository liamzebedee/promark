#include "glyph_atlas.h"
#include "gl_includes.h"
#include <cstring>

GlyphAtlas::GlyphAtlas(int width, int height)
    : atlasW(width), atlasH(height), texId(0), initialized(false),
      shelfY(0), shelfH(0), shelfX(0) {
}

GlyphAtlas::~GlyphAtlas() {
    if (texId) {
        glDeleteTextures(1, &texId);
    }
}

bool GlyphAtlas::init() {
    if (initialized) return true;

    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    // Allocate empty texture (GL_ALPHA for single-channel glyph data)
    std::vector<uint8_t> empty(atlasW * atlasH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlasW, atlasH, 0,
                 GL_ALPHA, GL_UNSIGNED_BYTE, empty.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    initialized = true;
    return true;
}

void GlyphAtlas::bind() {
    glBindTexture(GL_TEXTURE_2D, texId);
}

const AtlasGlyph* GlyphAtlas::get(uint32_t codepoint, int fontSize, TextStyle style, bool mono, FT_Face face) {
    AtlasKey key{codepoint, fontSize, static_cast<uint8_t>(style), mono};

    auto it = cache.find(key);
    if (it != cache.end()) {
        return &it->second;
    }

    // Set font size
    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Load and render glyph
    if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
        return nullptr;
    }

    FT_GlyphSlot g = face->glyph;

    AtlasGlyph glyph;
    glyph.width = g->bitmap.width;
    glyph.height = g->bitmap.rows;
    glyph.bearingX = g->bitmap_left;
    glyph.bearingY = g->bitmap_top;
    glyph.advance = g->advance.x >> 6;

    // Add to atlas if it has a bitmap
    if (g->bitmap.width > 0 && g->bitmap.rows > 0) {
        if (!addToAtlas(g->bitmap.width, g->bitmap.rows, g->bitmap.buffer,
                        glyph.u0, glyph.v0, glyph.u1, glyph.v1)) {
            // Atlas full
            return nullptr;
        }
    } else {
        // Space or empty glyph - no texture needed
        glyph.u0 = glyph.v0 = glyph.u1 = glyph.v1 = 0;
    }

    cache[key] = glyph;
    return &cache[key];
}

bool GlyphAtlas::addToAtlas(int w, int h, const uint8_t* bitmap,
                            float& u0, float& v0, float& u1, float& v1) {
    // Check if glyph fits in current shelf
    if (shelfX + w + PAD > atlasW) {
        // Move to next shelf
        shelfY += shelfH + PAD;
        shelfH = 0;
        shelfX = 0;
    }

    // Check if we need a new shelf and it fits
    if (shelfY + h + PAD > atlasH) {
        // Atlas is full
        return false;
    }

    // Update shelf height if this glyph is taller
    if (h > shelfH) {
        shelfH = h;
    }

    // Upload glyph bitmap to atlas
    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, shelfX, shelfY, w, h,
                    GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);

    // Calculate UV coordinates
    u0 = (float)shelfX / atlasW;
    v0 = (float)shelfY / atlasH;
    u1 = (float)(shelfX + w) / atlasW;
    v1 = (float)(shelfY + h) / atlasH;

    // Advance position
    shelfX += w + PAD;

    return true;
}
