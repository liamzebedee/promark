#pragma once
#include "paint_operations.h"
#include <map>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <jpeglib.h>

struct GlyphInfo {
    unsigned int textureID;
    int width;
    int height;
    int bearingX;
    int bearingY;
    int advance;
};

// Key for glyph cache: character + font size
struct GlyphKey {
    char character;
    int fontSize;

    bool operator<(const GlyphKey& other) const {
        if (character != other.character) return character < other.character;
        return fontSize < other.fontSize;
    }
};

class Rasterizer {
public:
    Rasterizer();
    ~Rasterizer();

    void rasterize(const DisplayList& displayList, const Rect& viewport);
    bool initializeFont();

private:
    struct ImageData {
        uint32_t width, height;
        std::vector<uint8_t> pixels;
        uint32_t textureId;
    };

    void executeDrawRect(const DrawRectOp& op);
    void executeDrawText(const DrawTextOp& op);
    void executeDrawImage(const DrawImageOp& op);
    void executeSetClip(const SetClipOp& op);
    void executeRestoreClip(const RestoreClipOp& op);
    void executeDrawDebugBorder(const DrawDebugBorderOp& op);
    void executeDrawCaret(const DrawCaretOp& op);
    void executeDrawSelectionRect(const DrawSelectionRectOp& op);

    void loadImage(const std::string& imagePath);
    void decodeJpeg(const std::string& filePath);
    void decodePng(const std::string& filePath);
    bool loadFromDataURI(const std::string& dataUri, ImageData& outData);
    bool decodeBase64(const std::string& base64, std::vector<uint8_t>& outBytes);
    bool decodePngFromMemory(const uint8_t* data, size_t length, ImageData& outData);
    bool decodeJpegFromMemory(const uint8_t* data, size_t length, ImageData& outData);

    std::map<std::string, ImageData> imageCache;
    Rect currentClip;
    bool hasClip;
    
    // FreeType font system
    FT_Library ft;
    FT_Face face;
    bool fontLoaded;
    
    // Font rendering
    bool loadFont(const char* fontPath);
    void renderChar(char c, float x, float y, const Color& color);
    void renderText(const std::string& text, float x, float y, const Color& color);

    // Glyph caching
    std::map<GlyphKey, GlyphInfo> glyphCache;
    const GlyphInfo* getGlyph(char c, int fontSize);
    int currentFontSize;
};