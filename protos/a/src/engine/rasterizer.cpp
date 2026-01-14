#include "rasterizer.h"
#include "utf8.h"
#include <map>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb/stb_image.h"

Rasterizer::Rasterizer() : faceRegular(nullptr), faceBold(nullptr),
    faceItalic(nullptr), faceBoldItalic(nullptr), faceMono(nullptr), fontLoaded(false),
    backend(nullptr) {
    initializeFont();
    loadMonoFont();
}

Rasterizer::~Rasterizer() {
    // Clean up any cached image textures via backend
    if (backend) {
        for (auto& pair : imageCache) {
            if (pair.second.textureId != 0) {
                backend->deleteTexture(pair.second.textureId);
            }
        }
    }

    // Clean up FreeType font resources
    if (fontLoaded) {
        if (faceRegular) FT_Done_Face(faceRegular);
        if (faceBold) FT_Done_Face(faceBold);
        if (faceItalic) FT_Done_Face(faceItalic);
        if (faceBoldItalic) FT_Done_Face(faceBoldItalic);
        if (faceMono) FT_Done_Face(faceMono);
        FT_Done_FreeType(ft);
    }
}

void Rasterizer::setBackend(RenderBackend* b) {
    backend = b;
}

void Rasterizer::rasterize(const PaintTree& paintTree, const Rect& viewport, float scrollOffsetY, bool caretVisible) {
    if (!backend) {
        std::cerr << "Rasterizer::rasterize called without backend set!" << std::endl;
        return;
    }

    backend->setScrollOffset(scrollOffsetY);

    // Traverse the paint tree recursively with viewport culling
    if (paintTree) {
        rasterizeArtifact(paintTree.get(), viewport, caretVisible);
    }

    backend->flush();
}

void Rasterizer::rasterizeDisplayList(const DisplayList& displayList, bool caretVisible) {
    // Legacy method for flat DisplayList - no culling
    for (const auto& op : displayList) {
        switch (op->getType()) {
            case PaintOpType::DrawRect:
                executeDrawRect(static_cast<const DrawRectOp&>(*op), caretVisible);
                break;
            case PaintOpType::DrawText:
                executeDrawText(static_cast<const DrawTextOp&>(*op));
                break;
            case PaintOpType::DrawImage:
                executeDrawImage(static_cast<const DrawImageOp&>(*op));
                break;
            case PaintOpType::DrawLine:
                executeDrawLine(static_cast<const DrawLineOp&>(*op));
                break;
        }
    }
}

void Rasterizer::rasterizeArtifact(const PaintArtifact* artifact, const Rect& viewport, bool caretVisible) {
    if (!artifact) return;

    // Viewport culling: skip entire subtree if bounds don't intersect viewport
    // Note: artifacts with zero-size bounds (like root) always pass this check
    if (artifact->bounds.size.width > 0 && artifact->bounds.size.height > 0) {
        if (!boundsIntersectViewport(artifact->bounds, viewport)) {
            return;  // Skip this artifact and all its children
        }
    }

    // Apply clip if present
    if (artifact->hasClip) {
        backend->pushClip(artifact->clipRect.position.x, artifact->clipRect.position.y,
                          artifact->clipRect.size.width, artifact->clipRect.size.height);
    }

    // Draw this artifact's display items
    for (const auto& op : artifact->displayItems) {
        switch (op->getType()) {
            case PaintOpType::DrawRect:
                executeDrawRect(static_cast<const DrawRectOp&>(*op), caretVisible);
                break;
            case PaintOpType::DrawText:
                executeDrawText(static_cast<const DrawTextOp&>(*op));
                break;
            case PaintOpType::DrawImage:
                executeDrawImage(static_cast<const DrawImageOp&>(*op));
                break;
            case PaintOpType::DrawLine:
                executeDrawLine(static_cast<const DrawLineOp&>(*op));
                break;
        }
    }

    // Recursively process children
    for (const auto& child : artifact->children) {
        rasterizeArtifact(child.get(), viewport, caretVisible);
    }

    // Restore clip if we pushed one
    if (artifact->hasClip) {
        backend->popClip();
    }
}

bool Rasterizer::boundsIntersectViewport(const Rect& bounds, const Rect& viewport) const {
    // Check if two axis-aligned rectangles intersect.
    // For scroll offset: viewport y is in screen coords, bounds y is in document coords.
    // The backend's scroll offset is already set, so we compare in document space.
    // Viewport in document space: y range is [scrollOffset, scrollOffset + viewport.height]

    float viewTop = viewport.position.y;  // This is actually the scroll offset in most cases
    float viewBottom = viewTop + viewport.size.height;
    float viewLeft = viewport.position.x;
    float viewRight = viewLeft + viewport.size.width;

    float boundsTop = bounds.position.y;
    float boundsBottom = boundsTop + bounds.size.height;
    float boundsLeft = bounds.position.x;
    float boundsRight = boundsLeft + bounds.size.width;

    // Rectangles intersect if they overlap on both axes
    bool xOverlap = boundsLeft < viewRight && boundsRight > viewLeft;
    bool yOverlap = boundsTop < viewBottom && boundsBottom > viewTop;

    return xOverlap && yOverlap;
}

void Rasterizer::executeDrawRect(const DrawRectOp& op, bool caretVisible) {
    const Rect& rect = op.getRect();
    const Color& color = op.getColor();
    RectRole role = op.getRole();

    float r = color.r / 255.0f, g = color.g / 255.0f;
    float b = color.b / 255.0f, a = color.a / 255.0f;

    switch (role) {
        case RectRole::Background:
        case RectRole::Selection:
            // Filled rectangles
            backend->drawRect(rect.position.x, rect.position.y,
                              rect.size.width, rect.size.height, r, g, b, a);
            break;

        case RectRole::Caret:
            // Only draw caret if visible (for blinking)
            if (caretVisible) {
                backend->drawRect(rect.position.x, rect.position.y,
                                  rect.size.width, rect.size.height, r, g, b, a);
            }
            break;

        case RectRole::Border:
            // Draw as stroke (4 thin rectangles)
            {
                float t = 1.0f;  // Border thickness
                // Top
                backend->drawRect(rect.position.x, rect.position.y, rect.size.width, t, r, g, b, a);
                // Bottom
                backend->drawRect(rect.position.x, rect.position.y + rect.size.height - t, rect.size.width, t, r, g, b, a);
                // Left
                backend->drawRect(rect.position.x, rect.position.y, t, rect.size.height, r, g, b, a);
                // Right
                backend->drawRect(rect.position.x + rect.size.width - t, rect.position.y, t, rect.size.height, r, g, b, a);
            }
            break;

        case RectRole::Debug:
            // Draw as debug border (thicker stroke)
            {
                float t = 2.0f;  // Debug border thickness
                // Top
                backend->drawRect(rect.position.x, rect.position.y, rect.size.width, t, r, g, b, a);
                // Bottom
                backend->drawRect(rect.position.x, rect.position.y + rect.size.height - t, rect.size.width, t, r, g, b, a);
                // Left
                backend->drawRect(rect.position.x, rect.position.y, t, rect.size.height, r, g, b, a);
                // Right
                backend->drawRect(rect.position.x + rect.size.width - t, rect.position.y, t, rect.size.height, r, g, b, a);
            }
            break;
    }
}

void Rasterizer::executeDrawText(const DrawTextOp& op) {
    if (!fontLoaded || !backend) {
        return;
    }

    const Point& position = op.getPosition();
    const std::string& text = op.getText();
    const Color& color = op.getColor();
    int fontSize = static_cast<int>(op.getFontSize());
    TextStyle style = op.getStyle();
    bool monospace = op.isMonospace();

    FT_Face face = getFaceForStyle(style, monospace);
    if (!face) return;

    backend->drawText(text, position.x, position.y,
                      color.r / 255.0f, color.g / 255.0f,
                      color.b / 255.0f, color.a / 255.0f,
                      fontSize, style, monospace, face);
}

void Rasterizer::executeDrawImage(const DrawImageOp& op) {
    if (!backend) return;

    const Rect& rect = op.getDestRect();
    const Rect& srcRect = op.getSourceRect();
    const Color& tint = op.getTintColor();

    // Determine which texture to use
    uint32_t textureId = op.getTextureId();
    if (textureId == 0) {
        // No pre-loaded texture, load from image path
        const std::string& imagePath = op.getImagePath();
        if (imageCache.find(imagePath) == imageCache.end()) {
            loadImage(imagePath);
        }
        if (imageCache.find(imagePath) != imageCache.end()) {
            textureId = imageCache[imagePath].textureId;
        }
    }

    if (textureId != 0) {
        backend->drawImage(rect.position.x, rect.position.y,
                           rect.size.width, rect.size.height,
                           textureId,
                           srcRect.position.x, srcRect.position.y,
                           srcRect.size.width, srcRect.size.height,
                           tint.r / 255.0f, tint.g / 255.0f,
                           tint.b / 255.0f, tint.a / 255.0f);
    }
}

void Rasterizer::loadImage(const std::string& imagePath) {
    if (!backend) return;

    ImageData imgData;
    bool loaded = false;

    // Check if this is a data URI
    if (imagePath.substr(0, 5) == "data:") {
        loaded = loadFromDataURI(imagePath, imgData);
    } else {
        // Load from file path using stb_image
        int width, height, channels;
        unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 4);
        if (data) {
            imgData.width = width;
            imgData.height = height;
            imgData.pixels.assign(data, data + width * height * 4);
            stbi_image_free(data);
            loaded = true;
        }
    }

    if (!loaded) {
        // Fallback to placeholder on decode failure
        imgData.width = 100;
        imgData.height = 100;
        imgData.pixels.resize(imgData.width * imgData.height * 4, 128);
    }

    // Create texture via backend (no direct GL calls)
    imgData.textureId = backend->createTexture(imgData.width, imgData.height,
                                                imgData.pixels.data(), true);

    imageCache[imagePath] = imgData;
}

bool Rasterizer::loadFromDataURI(const std::string& dataUri, ImageData& outData) {
    // Format: data:[<mediatype>][;base64],<data>
    size_t commaPos = dataUri.find(',');
    if (commaPos == std::string::npos) {
        return false;
    }

    std::string header = dataUri.substr(0, commaPos);
    std::string base64Data = dataUri.substr(commaPos + 1);

    // Check if base64 encoded
    bool isBase64 = header.find(";base64") != std::string::npos;
    if (!isBase64) {
        return false;  // Only base64 supported for now
    }

    // Decode base64
    std::vector<uint8_t> imageBytes;
    if (!decodeBase64(base64Data, imageBytes)) {
        return false;
    }

    // Determine image format and decode
    if (header.find("image/png") != std::string::npos) {
        return decodePngFromMemory(imageBytes.data(), imageBytes.size(), outData);
    } else if (header.find("image/jpeg") != std::string::npos ||
               header.find("image/jpg") != std::string::npos) {
        return decodeJpegFromMemory(imageBytes.data(), imageBytes.size(), outData);
    }

    // Fallback: unknown format
    return false;
}

bool Rasterizer::decodeBase64(const std::string& base64, std::vector<uint8_t>& outBytes) {
    static const std::string base64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Build lookup table
    std::vector<int> lookup(256, -1);
    for (int i = 0; i < 64; i++) {
        lookup[static_cast<unsigned char>(base64Chars[i])] = i;
    }

    outBytes.clear();
    outBytes.reserve(base64.size() * 3 / 4);

    uint32_t buffer = 0;
    int bitsCollected = 0;

    for (char c : base64) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;

        int value = lookup[static_cast<unsigned char>(c)];
        if (value < 0) {
            continue;  // Skip invalid characters
        }

        buffer = (buffer << 6) | value;
        bitsCollected += 6;

        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            outBytes.push_back(static_cast<uint8_t>((buffer >> bitsCollected) & 0xFF));
        }
    }

    return true;
}

bool Rasterizer::decodePngFromMemory(const uint8_t* data, size_t length, ImageData& outData) {
    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(data, length, &width, &height, &channels, 4);
    if (!pixels) {
        return false;
    }

    outData.width = width;
    outData.height = height;
    outData.pixels.assign(pixels, pixels + width * height * 4);
    stbi_image_free(pixels);
    return true;
}

void Rasterizer::decodeJpeg(const std::string& filePath) {
    // TODO: Implement JPEG file decoding
}

bool Rasterizer::decodeJpegFromMemory(const uint8_t* data, size_t length, ImageData& outData) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, data, length);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    // Request RGB output
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    outData.width = cinfo.output_width;
    outData.height = cinfo.output_height;
    outData.pixels.resize(outData.width * outData.height * 4);

    int rowStride = cinfo.output_width * cinfo.output_components;
    std::vector<uint8_t> rowBuffer(rowStride);

    size_t destRow = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        uint8_t* rowPtr = rowBuffer.data();
        jpeg_read_scanlines(&cinfo, &rowPtr, 1);

        // Convert RGB to RGBA
        for (uint32_t x = 0; x < outData.width; x++) {
            size_t destIdx = (destRow * outData.width + x) * 4;
            size_t srcIdx = x * 3;
            outData.pixels[destIdx + 0] = rowBuffer[srcIdx + 0];
            outData.pixels[destIdx + 1] = rowBuffer[srcIdx + 1];
            outData.pixels[destIdx + 2] = rowBuffer[srcIdx + 2];
            outData.pixels[destIdx + 3] = 255;
        }
        destRow++;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return true;
}

void Rasterizer::decodePng(const std::string& filePath) {
    // TODO: Implement PNG decoding
}

bool Rasterizer::initializeFont() {
    // Initialize FreeType
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Could not init FreeType Library" << std::endl;
        return false;
    }

#ifdef __EMSCRIPTEN__
    const char* fontPaths[] = { "/fonts/NotoSans-Regular.ttf" };
#elif defined(__APPLE__)
    const char* fontPaths[] = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Arial.ttf",
        "/Library/Fonts/Arial.ttf"
    };
#else
    // Linux: bundled fonts
    const char* fontPaths[] = { "fonts/NotoSans-Regular.ttf" };
#endif

    for (const char* fontPath : fontPaths) {
        if (loadFontFamily(fontPath)) {
            std::cout << "Loaded font family: " << fontPath << std::endl;
            return true;
        }
    }

    std::cerr << "Failed to load any system font" << std::endl;
    return false;
}

bool Rasterizer::loadFont(const char* fontPath, int faceIndex, FT_Face* outFace) {
    if (FT_New_Face(ft, fontPath, faceIndex, outFace)) {
        return false;
    }
    return true;
}

bool Rasterizer::loadFontFamily(const char* fontPath) {
    // Load regular face (index 0)
    if (!loadFont(fontPath, 0, &faceRegular)) {
        return false;
    }

#ifdef __EMSCRIPTEN__
    loadFont("/fonts/NotoSans-Bold.ttf", 0, &faceBold);
    loadFont("/fonts/NotoSans-Italic.ttf", 0, &faceItalic);
    loadFont("/fonts/NotoSans-BoldItalic.ttf", 0, &faceBoldItalic);
#elif defined(__APPLE__)
    // For TTC files, load bold/italic from different face indices
    std::string path(fontPath);
    if (path.length() > 4 && path.substr(path.length() - 4) == ".ttc") {
        loadFont(fontPath, 1, &faceBold);
        loadFont(fontPath, 2, &faceItalic);
        loadFont(fontPath, 3, &faceBoldItalic);
    }
#else
    // Linux: load separate bundled font files
    loadFont("fonts/NotoSans-Bold.ttf", 0, &faceBold);
    loadFont("fonts/NotoSans-Italic.ttf", 0, &faceItalic);
    loadFont("fonts/NotoSans-BoldItalic.ttf", 0, &faceBoldItalic);
#endif

    // Fallback: if bold/italic not loaded, use regular for all
    if (!faceBold) faceBold = faceRegular;
    if (!faceItalic) faceItalic = faceRegular;
    if (!faceBoldItalic) faceBoldItalic = faceBold ? faceBold : faceRegular;

    fontLoaded = true;
    return true;
}

bool Rasterizer::loadMonoFont() {
#ifdef __EMSCRIPTEN__
    const char* monoFontPaths[] = { "/fonts/NotoSansMono-Regular.ttf" };
#elif defined(__APPLE__)
    const char* monoFontPaths[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.dfont",
        "/Library/Fonts/Courier New.ttf"
    };
#else
    const char* monoFontPaths[] = { "fonts/NotoSansMono-Regular.ttf" };
#endif

    for (const char* monoPath : monoFontPaths) {
        if (loadFont(monoPath, 0, &faceMono)) {
            std::cout << "Rasterizer loaded mono font: " << monoPath << std::endl;
            return true;
        }
    }

    std::cerr << "Failed to load monospace font, using regular" << std::endl;
    faceMono = faceRegular;  // Fallback
    return false;
}

FT_Face Rasterizer::getFaceForStyle(TextStyle style, bool monospace) {
    if (!fontLoaded) return nullptr;

    // Use monospace font for code blocks
    if (monospace && faceMono) {
        return faceMono;
    }

    if (hasStyle(style, TextStyle::Bold) && hasStyle(style, TextStyle::Italic)) {
        return faceBoldItalic;
    } else if (hasStyle(style, TextStyle::Bold)) {
        return faceBold;
    } else if (hasStyle(style, TextStyle::Italic)) {
        return faceItalic;
    }
    return faceRegular;
}

void Rasterizer::executeDrawLine(const DrawLineOp& op) {
    if (!backend) return;

    const Point& start = op.getStart();
    const Point& end = op.getEnd();
    float thickness = op.getThickness();
    const Color& color = op.getColor();
    float r = color.r / 255.0f, g = color.g / 255.0f;
    float b = color.b / 255.0f, a = color.a / 255.0f;

    backend->drawLine(start.x, start.y, end.x, end.y, thickness, r, g, b, a);
}
