#pragma once
#include "paint_operations.h"
#include "render_backend.h"
#include <map>
#include <vector>
#include <memory>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <jpeglib.h>

// Rasterizer is a dispatcher that walks paint operations and calls RenderBackend methods.
// It no longer makes any direct GL calls - all rendering goes through the backend.
// The Rasterizer still owns:
// - FreeType font system (font faces are passed to backend for text rendering)
// - Image loading and caching (textures are created via backend)
//
// Spec reference: specs/04-rasterization.md - "The Rasterizer becomes a dispatcher"

class Rasterizer {
public:
    Rasterizer();
    ~Rasterizer();

    // Set the render backend (must be called before rasterize)
    void setBackend(RenderBackend* backend);

    // Rasterize a hierarchical PaintTree with viewport culling.
    // Entire subtrees whose bounds don't intersect the viewport are skipped.
    void rasterize(const PaintTree& paintTree, const Rect& viewport, float scrollOffsetY = 0.0f, bool caretVisible = true);

    // Legacy method: rasterize a flat DisplayList (for backwards compatibility)
    void rasterizeDisplayList(const DisplayList& displayList, bool caretVisible = true);
    bool initializeFont();

private:
    struct ImageData {
        uint32_t width, height;
        std::vector<uint8_t> pixels;
        uint32_t textureId;
    };

    // Recursive traversal of PaintTree with viewport culling
    void rasterizeArtifact(const PaintArtifact* artifact, const Rect& viewport, bool caretVisible);

    // Check if two rectangles intersect (for viewport culling)
    bool boundsIntersectViewport(const Rect& bounds, const Rect& viewport) const;

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

    // Render backend (owned externally)
    RenderBackend* backend;
};