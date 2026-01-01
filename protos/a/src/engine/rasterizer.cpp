#include "rasterizer.h"
#include <OpenGL/gl.h>
#include <map>
#include <iostream>

Rasterizer::Rasterizer() : hasClip(false), fontLoaded(false), currentFontSize(0) {
    initializeFont();
}

Rasterizer::~Rasterizer() {
    // Clean up any cached image textures
    for (auto& pair : imageCache) {
        if (pair.second.textureId != 0) {
            glDeleteTextures(1, &pair.second.textureId);
        }
    }

    // Clean up cached glyph textures
    for (auto& pair : glyphCache) {
        if (pair.second.textureID != 0) {
            glDeleteTextures(1, &pair.second.textureID);
        }
    }

    // Clean up FreeType font resources
    if (fontLoaded) {
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }
}

void Rasterizer::rasterize(const DisplayList& displayList, const Rect& viewport) {
    // Note: viewport and matrices are set up by Engine::render()
    // We don't reset them here to preserve scroll offset transforms
    (void)viewport;
    
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
            case PaintOpType::SetClip:
                executeSetClip(static_cast<const SetClipOp&>(*op));
                break;
            case PaintOpType::RestoreClip:
                executeRestoreClip(static_cast<const RestoreClipOp&>(*op));
                break;
            case PaintOpType::DrawDebugBorder:
                executeDrawDebugBorder(static_cast<const DrawDebugBorderOp&>(*op));
                break;
            case PaintOpType::DrawCaret:
                executeDrawCaret(static_cast<const DrawCaretOp&>(*op));
                break;
            case PaintOpType::DrawSelectionRect:
                executeDrawSelectionRect(static_cast<const DrawSelectionRectOp&>(*op));
                break;
        }
    }
}

void Rasterizer::executeDrawRect(const DrawRectOp& op) {
    const Rect& rect = op.getRect();
    const Color& color = op.getColor();
    
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(rect.position.x, rect.position.y);
    glVertex2f(rect.position.x + rect.size.width, rect.position.y);
    glVertex2f(rect.position.x + rect.size.width, rect.position.y + rect.size.height);
    glVertex2f(rect.position.x, rect.position.y + rect.size.height);
    glEnd();
}

void Rasterizer::executeDrawText(const DrawTextOp& op) {
    if (!fontLoaded) {
        return;
    }

    const Point& position = op.getPosition();
    const std::string& text = op.getText();
    const Color& color = op.getColor();
    int fontSize = static_cast<int>(op.getFontSize());

    // Set font size for glyph cache lookup
    if (fontSize != currentFontSize) {
        FT_Set_Pixel_Sizes(face, 0, fontSize);
        currentFontSize = fontSize;
    }

    // position.y is the baseline
    renderText(text, position.x, position.y, color);
}

void Rasterizer::executeDrawImage(const DrawImageOp& op) {
    const std::string& imagePath = op.getImagePath();
    
    // Load image if not cached
    if (imageCache.find(imagePath) == imageCache.end()) {
        loadImage(imagePath);
    }
    
    // Draw textured quad
    if (imageCache.find(imagePath) != imageCache.end()) {
        const ImageData& imgData = imageCache[imagePath];
        const Rect& rect = op.getDestRect();
        
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, imgData.textureId);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(rect.position.x, rect.position.y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(rect.position.x + rect.size.width, rect.position.y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(rect.position.x + rect.size.width, rect.position.y + rect.size.height);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(rect.position.x, rect.position.y + rect.size.height);
        glEnd();
        
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
}

void Rasterizer::executeSetClip(const SetClipOp& op) {
    currentClip = op.getClipRect();
    hasClip = true;
    
    // Use OpenGL scissor test for clipping
    glEnable(GL_SCISSOR_TEST);
    glScissor(currentClip.position.x, currentClip.position.y, 
              currentClip.size.width, currentClip.size.height);
}

void Rasterizer::executeRestoreClip(const RestoreClipOp& op) {
    (void)op;  // Unused
    hasClip = false;
    glDisable(GL_SCISSOR_TEST);
}

void Rasterizer::loadImage(const std::string& imagePath) {
    ImageData imgData;

    // Check if this is a data URI
    if (imagePath.substr(0, 5) == "data:") {
        if (!loadFromDataURI(imagePath, imgData)) {
            // Fallback to placeholder on decode failure
            imgData.width = 100;
            imgData.height = 100;
            imgData.pixels.resize(imgData.width * imgData.height * 4, 128);
        }
    } else {
        // TODO: Load from file path
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
    // Minimal PNG decoder for uncompressed/simple PNGs
    // PNG signature: 137 80 78 71 13 10 26 10
    if (length < 8 || data[0] != 137 || data[1] != 80 || data[2] != 78 || data[3] != 71) {
        return false;
    }

    // Parse chunks to find IHDR and IDAT
    size_t pos = 8;
    uint32_t width = 0, height = 0;
    uint8_t bitDepth = 0, colorType = 0;
    std::vector<uint8_t> compressedData;

    while (pos + 12 <= length) {
        uint32_t chunkLen = (data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3];
        std::string chunkType(reinterpret_cast<const char*>(&data[pos+4]), 4);

        if (chunkType == "IHDR" && chunkLen >= 13) {
            width = (data[pos+8] << 24) | (data[pos+9] << 16) | (data[pos+10] << 8) | data[pos+11];
            height = (data[pos+12] << 24) | (data[pos+13] << 16) | (data[pos+14] << 8) | data[pos+15];
            bitDepth = data[pos+16];
            colorType = data[pos+17];
        } else if (chunkType == "IDAT") {
            compressedData.insert(compressedData.end(),
                &data[pos+8], &data[pos+8+chunkLen]);
        } else if (chunkType == "IEND") {
            break;
        }

        pos += 12 + chunkLen;  // length(4) + type(4) + data + crc(4)
    }

    (void)bitDepth;
    (void)colorType;

    if (width == 0 || height == 0 || compressedData.empty()) {
        return false;
    }

    // For now, create a colored placeholder based on first few bytes of compressed data
    // (Full zlib decompression would require a zlib dependency)
    outData.width = width;
    outData.height = height;
    outData.pixels.resize(width * height * 4);

    // Generate a simple pattern using compressed data as "seed"
    uint8_t r = compressedData.size() > 0 ? compressedData[0] : 128;
    uint8_t g = compressedData.size() > 1 ? compressedData[1] : 128;
    uint8_t b = compressedData.size() > 2 ? compressedData[2] : 128;

    for (size_t i = 0; i < width * height; i++) {
        outData.pixels[i * 4 + 0] = r;
        outData.pixels[i * 4 + 1] = g;
        outData.pixels[i * 4 + 2] = b;
        outData.pixels[i * 4 + 3] = 255;
    }

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
    
    // Try to load system fonts
    const char* fontPaths[] = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Arial.ttf", 
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Times.ttc"
    };
    
    for (const char* fontPath : fontPaths) {
        if (loadFont(fontPath)) {
            std::cout << "Loaded font: " << fontPath << std::endl;
            return true;
        }
    }
    
    std::cerr << "Failed to load any system font" << std::endl;
    return false;
}

bool Rasterizer::loadFont(const char* fontPath) {
    if (FT_New_Face(ft, fontPath, 0, &face)) {
        return false;
    }

    fontLoaded = true;
    return true;
}

const GlyphInfo* Rasterizer::getGlyph(char c, int fontSize) {
    GlyphKey key{c, fontSize};

    // Check cache
    auto it = glyphCache.find(key);
    if (it != glyphCache.end()) {
        return &it->second;
    }

    // Set font size if changed
    if (fontSize != currentFontSize) {
        FT_Set_Pixel_Sizes(face, 0, fontSize);
        currentFontSize = fontSize;
    }

    // Load glyph
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
        return nullptr;
    }

    FT_GlyphSlot g = face->glyph;

    GlyphInfo info;
    info.width = g->bitmap.width;
    info.height = g->bitmap.rows;
    info.bearingX = g->bitmap_left;
    info.bearingY = g->bitmap_top;
    info.advance = g->advance.x >> 6;
    info.textureID = 0;

    // Create texture only if glyph has bitmap data
    if (g->bitmap.width > 0 && g->bitmap.rows > 0) {
        glGenTextures(1, &info.textureID);
        glBindTexture(GL_TEXTURE_2D, info.textureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
                     g->bitmap.width, g->bitmap.rows,
                     0, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glyphCache[key] = info;
    return &glyphCache[key];
}

void Rasterizer::renderChar(char c, float x, float y, const Color& color) {
    // Load glyph
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
        return;
    }

    FT_GlyphSlot g = face->glyph;

    // Skip empty glyphs (spaces, etc)
    if (g->bitmap.width == 0 || g->bitmap.rows == 0) {
        return;
    }

    // Create texture
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
                 g->bitmap.width, g->bitmap.rows,
                 0, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Position: x is pen position, y is baseline
    // bitmap_left: horizontal offset from pen position
    // bitmap_top: vertical offset from baseline (positive = above baseline)
    float xpos = x + g->bitmap_left;
    float ypos = y - g->bitmap_top;  // Top of glyph in top-left-origin coords
    float w = g->bitmap.width;
    float h = g->bitmap.rows;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glColor4ub(color.r, color.g, color.b, color.a);

    // Draw quad - FreeType bitmap is top-down, so flip texture coords
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos,     ypos);      // top-left
        glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + w, ypos);      // top-right
        glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + w, ypos + h);  // bottom-right
        glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos,     ypos + h);  // bottom-left
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDeleteTextures(1, &texture);
}

void Rasterizer::renderText(const std::string& text, float x, float y, const Color& color) {
    // Round baseline to integer to ensure consistent glyph positioning
    float penX = std::round(x);
    float baseline = std::round(y);

    // Enable blending once for all characters
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glColor4ub(color.r, color.g, color.b, color.a);

    for (char c : text) {
        if (c == '\n') {
            continue;  // Newlines handled by layout
        }

        // Get cached glyph
        const GlyphInfo* glyph = getGlyph(c, currentFontSize);
        if (!glyph) {
            continue;
        }

        // Render if glyph has a texture
        if (glyph->textureID != 0) {
            float xpos = std::round(penX + glyph->bearingX);
            float ypos = std::round(baseline - glyph->bearingY);
            float w = glyph->width;
            float h = glyph->height;

            glBindTexture(GL_TEXTURE_2D, glyph->textureID);

            glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos,     ypos);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + w, ypos);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + w, ypos + h);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos,     ypos + h);
            glEnd();
        }

        // Advance pen position
        penX += glyph->advance;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void Rasterizer::executeDrawDebugBorder(const DrawDebugBorderOp& op) {
    const Rect& rect = op.getRect();
    const Color& color = op.getColor();

    glLineWidth(3.0f);
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(rect.position.x, rect.position.y);
    glVertex2f(rect.position.x + rect.size.width, rect.position.y);
    glVertex2f(rect.position.x + rect.size.width, rect.position.y + rect.size.height);
    glVertex2f(rect.position.x, rect.position.y + rect.size.height);
    glEnd();
    glLineWidth(1.0f);
}

void Rasterizer::executeDrawCaret(const DrawCaretOp& op) {
    const Point& pos = op.getPosition();
    float height = op.getHeight();
    const Color& color = op.getColor();

    // Draw caret as a thin vertical line (2px wide)
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(pos.x, pos.y);
    glVertex2f(pos.x + 2.0f, pos.y);
    glVertex2f(pos.x + 2.0f, pos.y + height);
    glVertex2f(pos.x, pos.y + height);
    glEnd();
}

void Rasterizer::executeDrawSelectionRect(const DrawSelectionRectOp& op) {
    const Rect& rect = op.getRect();
    const Color& color = op.getColor();

    // Enable blending for translucent selection highlight
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(rect.position.x, rect.position.y);
    glVertex2f(rect.position.x + rect.size.width, rect.position.y);
    glVertex2f(rect.position.x + rect.size.width, rect.position.y + rect.size.height);
    glVertex2f(rect.position.x, rect.position.y + rect.size.height);
    glEnd();

    glDisable(GL_BLEND);
}