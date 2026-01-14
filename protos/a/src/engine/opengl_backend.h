#pragma once
#include "render_backend.h"
#include <vector>
#include <map>
#include <stack>

// Internal glyph representation for the atlas
struct BackendGlyph {
    float u0, v0, u1, v1;   // UV coordinates (normalized 0-1)
    int width, height;       // Glyph bitmap size
    int bearingX, bearingY;  // Offset from baseline
    int advance;             // Horizontal advance
};

struct BackendGlyphKey {
    uint32_t codepoint;
    int fontSize;
    uint8_t style;
    bool mono;

    bool operator<(const BackendGlyphKey& o) const {
        if (codepoint != o.codepoint) return codepoint < o.codepoint;
        if (fontSize != o.fontSize) return fontSize < o.fontSize;
        if (style != o.style) return style < o.style;
        return mono < o.mono;
    }
};

// OpenGLBackend implements RenderBackend using OpenGL 2.1/ES 2.0.
// This class is the SINGLE authority for all GL calls in the application.
// It absorbs:
// - GlyphAtlas: glyph texture management and caching
// - BatchRenderer: vertex batching, shaders, drawing
// - Frame setup GL calls from Engine
// - Image texture management from Rasterizer
//
// The glyph atlas and batch rendering are internal implementation details.

class OpenGLBackend : public RenderBackend {
public:
    OpenGLBackend();
    ~OpenGLBackend() override;

    // RenderBackend interface
    bool init() override;
    void beginFrame(int width, int height) override;
    void endFrame() override;
    void setViewport(int x, int y, int width, int height) override;
    void clear(float r, float g, float b, float a) override;
    void pushClip(int x, int y, int width, int height) override;
    void popClip() override;
    void drawRect(float x, float y, float width, float height,
                  float r, float g, float b, float a) override;
    void drawLine(float x1, float y1, float x2, float y2,
                  float thickness, float r, float g, float b, float a) override;
    void drawText(const std::string& text, float x, float y,
                  float r, float g, float b, float a,
                  int fontSize, TextStyle style, bool monospace,
                  FT_Face face) override;
    void drawImage(float x, float y, float width, float height,
                   uint32_t textureId,
                   float srcU0, float srcV0, float srcU1, float srcV1,
                   float tintR, float tintG, float tintB, float tintA) override;
    uint32_t createTexture(int width, int height, const uint8_t* pixels,
                           bool rgba = true) override;
    void deleteTexture(uint32_t textureId) override;
    void setScrollOffset(float offsetY) override;
    void flush() override;
    const char* getBackendName() const override { return "OpenGL 2.1"; }

private:
    // Vertex structure for batching
    struct Vertex {
        float x, y;       // Position
        float u, v;       // Texture coords
        float r, g, b, a; // Color
    };

    // Glyph atlas management (absorbed from GlyphAtlas)
    bool initAtlas();
    const BackendGlyph* getGlyph(uint32_t codepoint, int fontSize,
                                  TextStyle style, bool mono, FT_Face face);
    bool addGlyphToAtlas(int w, int h, const uint8_t* bitmap,
                         float& u0, float& v0, float& u1, float& v1);
    void bindAtlas();

    // Shader compilation
    bool compileShaders();
    unsigned int compileShader(unsigned int type, const char* source);
    unsigned int linkProgram(unsigned int vertShader, unsigned int fragShader,
                             const char* posAttr, const char* texAttr, const char* colorAttr);

    // Batch rendering (absorbed from BatchRenderer)
    void ensureCapacity(size_t numVertices);
    void drawQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  float r, float g, float b, float a);
    void updateProjection();

    // State
    bool initialized;
    int viewportW, viewportH;
    float scrollOffsetY;

    // Shaders
    unsigned int textProg;
    unsigned int solidProg;
    unsigned int imageProg;

    // Vertex buffer
    unsigned int vbo;
    std::vector<Vertex> vertices;
    static constexpr size_t MAX_VERTICES = 60000;  // 10000 quads
    bool texturedMode;  // Current batch mode (textured or solid)

    // Projection matrix
    float projMatrix[16];

    // Glyph atlas
    unsigned int atlasTexId;
    int atlasW, atlasH;
    int shelfY, shelfH, shelfX;
    static constexpr int ATLAS_PAD = 1;
    std::map<BackendGlyphKey, BackendGlyph> glyphCache;

    // Clip stack for nested scissor regions
    struct ClipRect {
        int x, y, width, height;
    };
    std::stack<ClipRect> clipStack;
};
