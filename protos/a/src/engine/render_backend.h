#pragma once
#include "layout_objects.h"
#include "paint_operations.h"
#include <string>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H

// RenderBackend is the single authority for all GPU rendering operations.
// This interface abstracts the underlying graphics API (OpenGL, Vulkan, etc.)
// enabling:
// - Testing with mock backends that don't require a real GL context
// - Future backend changes without touching rendering logic
// - Clear separation between rendering dispatch (Rasterizer) and GPU calls
//
// The Rasterizer walks paint operations and calls backend methods.
// The backend owns all GPU resources: textures, shaders, buffers.
// FreeType faces may be passed for text rendering but the backend owns
// the glyph cache/atlas internally.
//
// Spec reference: specs/04-rasterization.md - "Single GL Authority Rule"

class RenderBackend {
public:
    virtual ~RenderBackend() = default;

    // Initialization
    virtual bool init() = 0;

    // Frame lifecycle
    virtual void beginFrame(int width, int height) = 0;
    virtual void endFrame() = 0;

    // Viewport and clear
    virtual void setViewport(int x, int y, int width, int height) = 0;
    virtual void clear(float r, float g, float b, float a) = 0;

    // Clipping (scissor test)
    virtual void pushClip(int x, int y, int width, int height) = 0;
    virtual void popClip() = 0;

    // Drawing primitives
    virtual void drawRect(float x, float y, float width, float height,
                          float r, float g, float b, float a) = 0;

    virtual void drawLine(float x1, float y1, float x2, float y2,
                          float thickness, float r, float g, float b, float a) = 0;

    // Text rendering - backend owns glyph atlas internally
    virtual void drawText(const std::string& text, float x, float y,
                          float r, float g, float b, float a,
                          int fontSize, TextStyle style, bool monospace,
                          FT_Face face) = 0;

    // Image rendering
    virtual void drawImage(float x, float y, float width, float height,
                           uint32_t textureId,
                           float srcU0, float srcV0, float srcU1, float srcV1,
                           float tintR, float tintG, float tintB, float tintA) = 0;

    // Texture management - backend owns all textures
    virtual uint32_t createTexture(int width, int height, const uint8_t* pixels,
                                   bool rgba = true) = 0;
    virtual void deleteTexture(uint32_t textureId) = 0;

    // Batch control - for batching consecutive operations
    virtual void setScrollOffset(float offsetY) = 0;
    virtual void flush() = 0;  // Flush any pending batched operations

    // Info for debugging
    virtual const char* getBackendName() const = 0;
};
