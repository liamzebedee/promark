#pragma once
#include "text_buffer.h"
#include "markdown_parser.h"
#include "layout_engine.h"
#include "painter.h"
#include "rasterizer.h"
#include "render_backend.h"
#include "font_provider.h"
#include <memory>
#include <cstdint>

// Caret/selection state (similar to Blink's FrameSelection)
struct CaretState {
    int cursorPosition = 0;
    int selectionStart = 0;
    int selectionEnd = 0;
    bool hasSelection = false;
    bool caretVisible = true;  // For blinking
    float animatedCaretX = 0;  // Animated position
    float animatedCaretY = 0;
    bool useAnimatedPosition = false;
};

class MarkdownRenderer {
public:
    MarkdownRenderer();
    ~MarkdownRenderer();

    // Set text buffer reference (does NOT take ownership)
    // MarkdownRenderer observes the TextBuffer and uses version tracking
    // to detect when re-parsing is needed
    void setTextBuffer(const TextBuffer* buffer);

    // Legacy method for compatibility during transition
    void setTextBuffer(std::unique_ptr<TextBuffer> buffer);

    void setCaretState(const CaretState& state);
    void setFontProvider(const FontProvider* provider);
    void setBackend(RenderBackend* backend);  // Set render backend for rasterization
    // Render with scroll support:
    // - documentScrollY: scroll position in document space (for viewport culling)
    // - gpuScrollOffset: offset applied to GPU rendering (TOOLBAR_HEIGHT - documentScrollY)
    void render(const Size& viewportSize, float documentScrollY = 0.0f, float gpuScrollOffset = 0.0f);

    // Force layout computation without painting (used to get accurate cursor position)
    void ensureLayoutValid(const Size& viewportSize);

    // Manual pipeline control for debugging/testing
    void parseMarkdown();
    void performLayout(const Size& availableSpace);
    void paint();
    void rasterize(const Size& viewportSize, float documentScrollY = 0.0f, float gpuScrollOffset = 0.0f);

    const MarkdownObject* getObjectTree() const;
    const LayoutObject* getLayoutTree() const;
    const PaintTree& getPaintTree() const;
    float getContentHeight() const;

    // DOM↔Raw position mapping
    int getTotalDOMLength() const;
    int domToRaw(int domPos) const;
    int rawToDOM(int rawPos) const;

    // Hit testing - convert screen coordinates to raw cursor position
    int hitTest(float x, float y) const;

    // Get cursor Y position for auto-scroll
    float getCursorY(int domPos) const;

    // Get cursor X/Y position for animation
    // preferCurrentAtBoundary: at exact layout boundaries, prefer current layout (for caret after typing)
    //                          vs next layout (for navigation). Default false for backward compat.
    void getCursorXY(int domPos, float& outX, float& outY, bool preferCurrentAtBoundary = false) const;

    // Get link URL at position (returns empty string if not on a link)
    std::string getLinkAtPosition(float x, float y) const;

private:
    // Non-owning pointer to the authoritative TextBuffer
    const TextBuffer* textBuffer;

    // Owned copy for legacy compatibility (will be removed in future)
    std::unique_ptr<TextBuffer> ownedTextBuffer;

    uint64_t lastTextVersion;  // Track version to detect changes

    std::unique_ptr<MarkdownParser> parser;
    std::unique_ptr<LayoutEngine> layoutEngine;
    std::unique_ptr<Painter> painter;
    std::unique_ptr<Rasterizer> rasterizer;

    std::unique_ptr<MarkdownObject> objectTree;
    std::unique_ptr<LayoutObject> layoutTree;
    PaintTree paintTree;  // Hierarchical paint artifact tree

    CaretState caretState;

    bool needsReparse;
    bool needsRelayout;
    bool needsRepaint;

    Size lastViewportSize;
};