#pragma once
#include "text_buffer.h"
#include "markdown_parser.h"
#include "layout_engine.h"
#include "painter.h"
#include "rasterizer.h"
#include <memory>

class MarkdownRenderer {
public:
    MarkdownRenderer();
    ~MarkdownRenderer();
    
    void setTextBuffer(std::unique_ptr<TextBuffer> buffer);
    void render(const Size& viewportSize);
    
    // Manual pipeline control for debugging/testing
    void parseMarkdown();
    void performLayout(const Size& availableSpace);
    void paint();
    void rasterize(const Size& viewportSize);
    
    const MarkdownObject* getObjectTree() const;
    const LayoutObject* getLayoutTree() const;
    const DisplayList& getDisplayList() const;
    
private:
    std::unique_ptr<TextBuffer> textBuffer;
    std::unique_ptr<MarkdownParser> parser;
    std::unique_ptr<LayoutEngine> layoutEngine;
    std::unique_ptr<Painter> painter;
    std::unique_ptr<Rasterizer> rasterizer;
    
    std::unique_ptr<MarkdownObject> objectTree;
    std::unique_ptr<LayoutObject> layoutTree;
    DisplayList displayList;
    
    bool needsReparse;
    bool needsRelayout;
    bool needsRepaint;
};