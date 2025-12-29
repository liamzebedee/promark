#include "markdown_renderer.h"

MarkdownRenderer::MarkdownRenderer() 
    : needsReparse(true), needsRelayout(true), needsRepaint(true) {
    parser = std::make_unique<MarkdownParser>();
    layoutEngine = std::make_unique<LayoutEngine>();
    painter = std::make_unique<Painter>();
    rasterizer = std::make_unique<Rasterizer>();
}

MarkdownRenderer::~MarkdownRenderer() {
}

void MarkdownRenderer::setTextBuffer(std::unique_ptr<TextBuffer> buffer) {
    textBuffer = std::move(buffer);
    needsReparse = true;
    needsRelayout = true;
    needsRepaint = true;
}

void MarkdownRenderer::render(const Size& viewportSize) {
    if (needsReparse) {
        parseMarkdown();
    }
    
    if (needsRelayout) {
        performLayout(viewportSize);
    }
    
    if (needsRepaint) {
        paint();
    }
    
    rasterize(viewportSize);
}

void MarkdownRenderer::parseMarkdown() {
    if (!textBuffer) {
        return;
    }
    
    objectTree = parser->parse(*textBuffer);
    needsReparse = false;
    needsRelayout = true;
    needsRepaint = true;
}

void MarkdownRenderer::performLayout(const Size& availableSpace) {
    if (!objectTree) {
        return;
    }
    
    layoutTree = layoutEngine->createLayoutTree(objectTree.get());
    if (layoutTree) {
        layoutEngine->performLayout(layoutTree.get(), availableSpace);
    }
    
    needsRelayout = false;
    needsRepaint = true;
}

void MarkdownRenderer::paint() {
    if (!layoutTree) {
        return;
    }
    
    displayList = painter->paint(layoutTree.get());
    needsRepaint = false;
}

void MarkdownRenderer::rasterize(const Size& viewportSize) {
    Rect viewport(0, 0, viewportSize.width, viewportSize.height);
    rasterizer->rasterize(displayList, viewport);
}

const MarkdownObject* MarkdownRenderer::getObjectTree() const {
    return objectTree.get();
}

const LayoutObject* MarkdownRenderer::getLayoutTree() const {
    return layoutTree.get();
}

const DisplayList& MarkdownRenderer::getDisplayList() const {
    return displayList;
}