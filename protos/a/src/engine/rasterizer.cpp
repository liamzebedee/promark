#include "rasterizer.h"
#include <OpenGL/gl.h>
#include <map>
#include <iostream>

Rasterizer::Rasterizer() : hasClip(false), fontLoaded(false) {
    initializeFont();
}

Rasterizer::~Rasterizer() {
    // Clean up any cached textures
    for (auto& pair : imageCache) {
        if (pair.second.textureId != 0) {
            glDeleteTextures(1, &pair.second.textureId);
        }
    }
    
    // Clean up FreeType font resources
    if (fontLoaded) {
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }
}

void Rasterizer::rasterize(const DisplayList& displayList, const Rect& viewport) {
    // Set up viewport
    glViewport(viewport.position.x, viewport.position.y, viewport.size.width, viewport.size.height);
    
    // Set up orthographic projection for 2D rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewport.size.width, viewport.size.height, 0, -1, 1); // Top-left origin
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
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
    float fontSize = op.getFontSize();
    
    // Set font size
    FT_Set_Pixel_Sizes(face, 0, (int)fontSize);
    
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
    // TODO: Load and decode image file
    // Determine image format and call appropriate decoder
    
    ImageData imgData;
    imgData.width = 100;
    imgData.height = 100;
    imgData.pixels.resize(imgData.width * imgData.height * 4, 128); // Gray placeholder
    
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

void Rasterizer::decodeJpeg(const std::string& filePath) {
    // TODO: Implement JPEG decoding
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


void Rasterizer::renderChar(char c, float x, float y, const Color& color) {
    // Load glyph directly from FreeType each time
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
        return; // Failed to load glyph
    }

    // Skip empty glyphs (like spaces)
    if (face->glyph->bitmap.width == 0 || face->glyph->bitmap.rows == 0) {
        return;
    }

    // Create temporary texture for this glyph
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_ALPHA,
        face->glyph->bitmap.width,
        face->glyph->bitmap.rows,
        0,
        GL_ALPHA,
        GL_UNSIGNED_BYTE,
        face->glyph->bitmap.buffer
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Render the glyph
    float xpos = x + face->glyph->bitmap_left;
    float ypos = y - face->glyph->bitmap_top;
    float w = face->glyph->bitmap.width;
    float h = face->glyph->bitmap.rows;
    
    // Enable blending for text
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glColor4ub(color.r, color.g, color.b, color.a);
    
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos, ypos + h);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + w, ypos + h); 
        glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + w, ypos);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos, ypos);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    
    // Clean up temporary texture
    glDeleteTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Rasterizer::renderText(const std::string& text, float x, float y, const Color& color) {
    float currentX = x;
    float currentY = y;
    
    for (const char* p = text.c_str(); *p; p++) {
        if (*p == '\n') {
            currentY += 24;
            currentX = x;
            continue;
        }
        
        renderChar(*p, currentX, currentY, color);
        
        // Advance cursor - load char to get advance
        if (FT_Load_Char(face, *p, FT_LOAD_DEFAULT) == 0) {
            currentX += face->glyph->advance.x >> 6;
        }
    }
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