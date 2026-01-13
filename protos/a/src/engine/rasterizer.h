#pragma once
#include "paint_operations.h"
#include "glyph_atlas.h"
#include "batch_renderer.h"
#include <map>
#include <vector>
#include <memory>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <jpeglib.h>

class Rasterizer {
public:
    Rasterizer();
    ~Rasterizer();

    void rasterize(const DisplayList& displayList, const Rect& viewport, float scrollOffsetY = 0.0f, bool caretVisible = true);
    bool initializeFont();

private:
    struct ImageData {
        uint32_t width, height;
        std::vector<uint8_t> pixels;
        uint32_t textureId;
    };

    void executeDrawRect(const DrawRectOp& op, bool caretVisible);
    void executeDrawText(const DrawTextOp& op);
    void executeDrawImage(const DrawImageOp& op);
    void executeDrawLine(const DrawLineOp& op);

    void loadImage(const std::string& imagePath);
    void decodeJpeg(const std::string& filePath);
    void decodePng(const std::string& filePath);
    bool loadFromDataURI(const std::string& dataUri, ImageData& outData);
    bool decodeBase64(const std::string& base64, std::vector<uint8_t>& outBytes);
    bool decodePngFromMemory(const uint8_t* data, size_t length, ImageData& outData);
    bool decodeJpegFromMemory(const uint8_t* data, size_t length, ImageData& outData);

    std::map<std::string, ImageData> imageCache;

    // FreeType font system
    FT_Library ft;
    FT_Face faceRegular;
    FT_Face faceBold;
    FT_Face faceItalic;
    FT_Face faceBoldItalic;
    FT_Face faceMono;
    bool fontLoaded;

    // Font rendering
    bool loadFont(const char* fontPath, int faceIndex, FT_Face* outFace);
    bool loadFontFamily(const char* fontPath);
    bool loadMonoFont();
    FT_Face getFaceForStyle(TextStyle style, bool monospace);

    // GL2 renderer
    std::unique_ptr<GlyphAtlas> atlas;
    std::unique_ptr<BatchRenderer> batchRenderer;
    bool gl2Initialized;
};