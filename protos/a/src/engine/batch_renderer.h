#pragma once
#include "glyph_atlas.h"
#include "paint_operations.h"
#include <vector>
#include <string>

struct Vertex {
    float x, y;       // Position
    float u, v;       // Texture coords
    float r, g, b, a; // Color
};

class BatchRenderer {
public:
    BatchRenderer();
    ~BatchRenderer();

    bool init();
    void setViewport(int width, int height, float scrollOffsetY = 0.0f);

    // Begin/end frame
    void begin();
    void flush();  // Draw accumulated geometry

    // Drawing primitives
    void drawQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  float r, float g, float b, float a);
    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a);
    void drawImage(float x, float y, float w, float h, unsigned int textureId,
                   float srcU0, float srcV0, float srcU1, float srcV1,
                   float tintR, float tintG, float tintB, float tintA);

    // Text rendering with atlas
    void drawText(const std::string& text, float x, float y,
                  float r, float g, float b, float a,
                  int fontSize, TextStyle style, bool mono, FT_Face face);

    // Set glyph atlas (must be called before drawText)
    void setAtlas(GlyphAtlas* atlas) { glyphAtlas = atlas; }

private:
    void ensureCapacity(size_t numVertices);

    unsigned int textProg;
    unsigned int solidProg;
    unsigned int imageProg;
    unsigned int vbo;

    std::vector<Vertex> vertices;
    static constexpr size_t MAX_VERTICES = 60000;  // 10000 quads

    float projMatrix[16];
    int viewportW, viewportH;

    GlyphAtlas* glyphAtlas;
    bool textured;  // Current batch mode
};
