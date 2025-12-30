#pragma once
#include "paint_operations.h"
#include <map>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H

struct Glyph {
    unsigned int textureID;
    int width;
    int height;
    int bearingX;
    int bearingY;
    int advance;
};

class Rasterizer {
public:
    Rasterizer();
    ~Rasterizer();
    
    void rasterize(const DisplayList& displayList, const Rect& viewport);
    bool initializeFont();
    
private:
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
    
    struct ImageData {
        uint32_t width, height;
        std::vector<uint8_t> pixels;
        uint32_t textureId;
    };
    
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
};