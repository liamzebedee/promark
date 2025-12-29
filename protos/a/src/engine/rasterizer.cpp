#include "rasterizer.h"
#include <OpenGL/gl.h>
#include <map>

Rasterizer::Rasterizer() : hasClip(false) {
}

Rasterizer::~Rasterizer() {
    // Clean up any cached textures
    for (auto& pair : imageCache) {
        if (pair.second.textureId != 0) {
            glDeleteTextures(1, &pair.second.textureId);
        }
    }
}

void Rasterizer::rasterize(const DisplayList& displayList, const Rect& viewport) {
    // Set up viewport
    glViewport(viewport.position.x, viewport.position.y, viewport.size.width, viewport.size.height);
    
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
        }
    }
}

void Rasterizer::executeDrawRect(const DrawRectOp& op) {
    // TODO: Draw rectangle using OpenGL
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
    // TODO: Draw text using font system
    // For now, this is a placeholder - actual text rendering would use
    // the existing FreeType glyph system from engine.cpp
}

void Rasterizer::executeDrawImage(const DrawImageOp& op) {
    // TODO: Draw image using OpenGL textures
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
    // TODO: Set clipping rectangle
    currentClip = op.getClipRect();
    hasClip = true;
    
    // Use OpenGL scissor test for clipping
    glEnable(GL_SCISSOR_TEST);
    glScissor(currentClip.position.x, currentClip.position.y, 
              currentClip.size.width, currentClip.size.height);
}

void Rasterizer::executeRestoreClip(const RestoreClipOp& op) {
    // TODO: Restore previous clipping state
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