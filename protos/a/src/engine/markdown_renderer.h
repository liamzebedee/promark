#pragma once
#include "text_buffer.h"
#include "markdown_parser.h"
#include "layout_engine.h"
#include "painter.h"
#include "rasterizer.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <memory>

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

    void setTextBuffer(std::unique_ptr<TextBuffer> buffer);
    void setCaretState(const CaretState& state);
    void setFontFace(FT_Face face);
    void render(const Size& viewportSize);

    // Manual pipeline control for debugging/testing
    void parseMarkdown();
    void performLayout(const Size& availableSpace);
    void paint();
    void rasterize(const Size& viewportSize);

    const MarkdownObject* getObjectTree() const;
    const LayoutObject* getLayoutTree() const;
    const DisplayList& getDisplayList() const;
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
    void getCursorXY(int domPos, float& outX, float& outY) const;

    // Get link URL at position (returns empty string if not on a link)
    std::string getLinkAtPosition(float x, float y) const;

private:
    std::unique_ptr<TextBuffer> textBuffer;
    std::unique_ptr<MarkdownParser> parser;
    std::unique_ptr<LayoutEngine> layoutEngine;
    std::unique_ptr<Painter> painter;
    std::unique_ptr<Rasterizer> rasterizer;

    std::unique_ptr<MarkdownObject> objectTree;
    std::unique_ptr<LayoutObject> layoutTree;
    DisplayList displayList;

    CaretState caretState;

    bool needsReparse;
    bool needsRelayout;
    bool needsRepaint;

    Size lastViewportSize;
};