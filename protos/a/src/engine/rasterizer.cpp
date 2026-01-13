#include "rasterizer.h"
#include "utf8.h"
#include "gl_includes.h"
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
    gl2Initialized(false) {
    initializeFont();
    loadMonoFont();

    // Initialize GL2 renderer
    atlas = std::make_unique<GlyphAtlas>(1024, 1024);
    batchRenderer = std::make_unique<BatchRenderer>();
}

Rasterizer::~Rasterizer() {
    // Clean up any cached image textures
    for (auto& pair : imageCache) {
        if (pair.second.textureId != 0) {
            glDeleteTextures(1, &pair.second.textureId);
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

void Rasterizer::rasterize(const DisplayList& displayList, const Rect& viewport, float scrollOffsetY, bool caretVisible) {
    // Initialize GL2 renderer on first use (need OpenGL context to be ready)
    if (!gl2Initialized) {
        atlas->init();
        batchRenderer->init();
        batchRenderer->setAtlas(atlas.get());
        gl2Initialized = true;
    }

    batchRenderer->setViewport(viewport.size.width, viewport.size.height, scrollOffsetY);
    batchRenderer->begin();

    // Execute all paint operations
    for (const auto& op : displayList) {
        switch (op->getType()) {
            case PaintOpType::DrawRect:
                executeDrawRect(static_cast<const DrawRectOp&>(*op));
                break;
            case PaintOpType::DrawText:
                executeDrawText(static_cast<const DrawTextOp&>(*op));
                break;
            case PaintOpType::DrawImage:
                executeDrawImage(static_cast<const DrawImageOp&>(*op));
                break;
            case PaintOpType::DrawDebugBorder:
                executeDrawDebugBorder(static_cast<const DrawDebugBorderOp&>(*op));
                break;
            case PaintOpType::DrawCaret:
                // Only draw caret if visible (for blinking)
                if (caretVisible) {
                    executeDrawCaret(static_cast<const DrawCaretOp&>(*op));
                }
                break;
            case PaintOpType::DrawSelectionRect:
                executeDrawSelectionRect(static_cast<const DrawSelectionRectOp&>(*op));
                break;
            case PaintOpType::DrawLine:
                executeDrawLine(static_cast<const DrawLineOp&>(*op));
                break;
        }
    }

    batchRenderer->flush();
}

void Rasterizer::executeDrawRect(const DrawRectOp& op) {
    const Rect& rect = op.getRect();
    const Color& color = op.getColor();

    batchRenderer->drawRect(rect.position.x, rect.position.y,
                            rect.size.width, rect.size.height,
                            color.r / 255.0f, color.g / 255.0f,
                            color.b / 255.0f, color.a / 255.0f);
}

void Rasterizer::executeDrawText(const DrawTextOp& op) {
    if (!fontLoaded) {
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

    batchRenderer->drawText(text, position.x, position.y,
                            color.r / 255.0f, color.g / 255.0f,
                            color.b / 255.0f, color.a / 255.0f,
                            fontSize, style, monospace, face);
}

void Rasterizer::executeDrawImage(const DrawImageOp& op) {
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
        batchRenderer->drawImage(rect.position.x, rect.position.y,
                                  rect.size.width, rect.size.height,
                                  textureId,
                                  srcRect.position.x, srcRect.position.y,
                                  srcRect.size.width, srcRect.size.height,
                                  tint.r / 255.0f, tint.g / 255.0f,
                                  tint.b / 255.0f, tint.a / 255.0f);
    }
}

void Rasterizer::loadImage(const std::string& imagePath) {
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

    // Create OpenGL texture
    glGenTextures(1, &imgData.textureId);
    glBindTexture(GL_TEXTURE_2D, imgData.textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgData.width, imgData.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, imgData.pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

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

void Rasterizer::executeDrawDebugBorder(const DrawDebugBorderOp& op) {
    const Rect& rect = op.getRect();
    const Color& color = op.getColor();
    float r = color.r / 255.0f, g = color.g / 255.0f;
    float b = color.b / 255.0f, a = color.a / 255.0f;
    float t = 2.0f;  // Border thickness

    // Top
    batchRenderer->drawRect(rect.position.x, rect.position.y, rect.size.width, t, r, g, b, a);
    // Bottom
    batchRenderer->drawRect(rect.position.x, rect.position.y + rect.size.height - t, rect.size.width, t, r, g, b, a);
    // Left
    batchRenderer->drawRect(rect.position.x, rect.position.y, t, rect.size.height, r, g, b, a);
    // Right
    batchRenderer->drawRect(rect.position.x + rect.size.width - t, rect.position.y, t, rect.size.height, r, g, b, a);
}

void Rasterizer::executeDrawCaret(const DrawCaretOp& op) {
    const Point& pos = op.getPosition();
    float height = op.getHeight();
    const Color& color = op.getColor();

    batchRenderer->drawRect(pos.x, pos.y, 2.0f, height,
                            color.r / 255.0f, color.g / 255.0f,
                            color.b / 255.0f, color.a / 255.0f);
}

void Rasterizer::executeDrawSelectionRect(const DrawSelectionRectOp& op) {
    const Rect& rect = op.getRect();
    const Color& color = op.getColor();

    batchRenderer->drawRect(rect.position.x, rect.position.y,
                            rect.size.width, rect.size.height,
                            color.r / 255.0f, color.g / 255.0f,
                            color.b / 255.0f, color.a / 255.0f);
}

void Rasterizer::executeDrawLine(const DrawLineOp& op) {
    const Point& start = op.getStart();
    const Point& end = op.getEnd();
    float thickness = op.getThickness();
    const Color& color = op.getColor();
    float r = color.r / 255.0f, g = color.g / 255.0f;
    float b = color.b / 255.0f, a = color.a / 255.0f;

    // Draw line as a thin rectangle
    float dx = end.x - start.x;
    float dy = end.y - start.y;

    if (std::abs(dx) > std::abs(dy)) {
        // Horizontal-ish line
        float minX = std::min(start.x, end.x);
        float y = start.y - thickness / 2.0f;
        batchRenderer->drawRect(minX, y, std::abs(dx), thickness, r, g, b, a);
    } else {
        // Vertical-ish line
        float minY = std::min(start.y, end.y);
        float x = start.x - thickness / 2.0f;
        batchRenderer->drawRect(x, minY, thickness, std::abs(dy), r, g, b, a);
    }
}