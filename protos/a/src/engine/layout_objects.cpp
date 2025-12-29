#include "layout_objects.h"
#include <iostream>

LayoutObject::LayoutObject(const MarkdownObject* sourceObject, LayoutFlow flow) 
    : sourceObject(sourceObject), flow(flow), parent(nullptr) {
}

LayoutObject::~LayoutObject() {
}

const MarkdownObject* LayoutObject::getSourceObject() const {
    return sourceObject;
}

LayoutFlow LayoutObject::getFlow() const {
    return flow;
}

void LayoutObject::setRect(const Rect& rect) {
    this->rect = rect;
}

const Rect& LayoutObject::getRect() const {
    return rect;
}

void LayoutObject::addChild(std::unique_ptr<LayoutObject> child) {
    child->setParent(this);
    children.push_back(std::move(child));
}

const std::vector<std::unique_ptr<LayoutObject>>& LayoutObject::getChildren() const {
    return children;
}

Size LayoutObject::computeIntrinsicSize() const {
    // TODO: Implement intrinsic size computation
    return Size(0, 0);
}

void LayoutObject::layout(const Size& availableSpace) {
    // TODO: Implement base layout
}

float LayoutObject::getFontSize() const {
    // Base implementation - different object types override this
    switch (sourceObject->getType()) {
        case MarkdownObjectType::Heading: {
            const HeadingObject* heading = static_cast<const HeadingObject*>(sourceObject);
            switch (heading->getLevel()) {
                case 1: return 32.0f;
                case 2: return 28.0f;
                case 3: return 24.0f;
                case 4: return 20.0f;
                case 5: return 18.0f;
                case 6: return 16.0f;
                default: return 24.0f;
            }
        }
        case MarkdownObjectType::Text:
            // Text inherits font size from parent
            if (parent) {
                return parent->getFontSize();
            }
            return 16.0f; // Default body text
        default:
            return 16.0f; // Default
    }
}

void LayoutObject::setParent(LayoutObject* parent) {
    this->parent = parent;
}

LayoutObject* LayoutObject::getParent() const {
    return parent;
}

BlockLayoutObject::BlockLayoutObject(const MarkdownObject* sourceObject) 
    : LayoutObject(sourceObject, LayoutFlow::Block) {
}

void BlockLayoutObject::layout(const Size& availableSpace) {
    // Simple block layout - stack children vertically
    float currentY = 100; // Start well away from top edge
    float leftMargin = 100; // Start well away from left edge
    
    std::cout << "BlockLayout: positioning " << children.size() << " children" << std::endl;
    
    for (const auto& child : children) {
        // Give each child full width, height based on content
        Size childAvailableSpace(availableSpace.width - leftMargin, availableSpace.height - currentY);
        child->layout(childAvailableSpace);
        
        const Rect& childRect = child->getRect();
        // Set absolute position for child
        Rect newRect(leftMargin, currentY, childRect.size.width, childRect.size.height);
        child->setRect(newRect);
        
        std::cout << "BlockLayout: set child rect to (" << leftMargin << "," << currentY << "," << childRect.size.width << "," << childRect.size.height << ")" << std::endl;
        
        currentY += childRect.size.height + 30; // Add more spacing between blocks
    }
    
    setRect(Rect(0, 0, availableSpace.width, currentY));
}

InlineLayoutObject::InlineLayoutObject(const MarkdownObject* sourceObject) 
    : LayoutObject(sourceObject, LayoutFlow::Inline) {
}

void InlineLayoutObject::layout(const Size& availableSpace) {
    // TODO: Implement inline layout (horizontal flow with line breaking)
}

TextLayoutObject::TextLayoutObject(const MarkdownObject* sourceObject) 
    : LayoutObject(sourceObject, LayoutFlow::Inline) {
}

Size TextLayoutObject::computeIntrinsicSize() const {
    // Get text and compute size based on parent object type
    std::string text = sourceObject->getText();
    if (text.empty()) {
        return Size(0, 0);
    }
    
    float fontSize = getFontSize();
    float charWidth = fontSize * 0.6f; // Rough character width estimate
    float lineHeight = fontSize * 1.2f; // Line height with some spacing
    
    // Simple width calculation (actual implementation would use font metrics)
    float width = text.length() * charWidth;
    
    return Size(width, lineHeight);
}

float TextLayoutObject::getFontSize() const {
    // Text objects inherit font size from their parent layout object
    if (parent) {
        return parent->getFontSize();
    }
    return 16.0f; // Default body text
}

void TextLayoutObject::layout(const Size& availableSpace) {
    // Compute text size and set rect
    Size textSize = computeIntrinsicSize();
    setRect(Rect(0, 0, textSize.width, textSize.height));
    shapeText();
}

const std::vector<TextLayoutObject::GlyphRun>& TextLayoutObject::getGlyphRuns() const {
    return glyphRuns;
}

void TextLayoutObject::shapeText() {
    // TODO: Use HarfBuzz to shape text into glyphs
    // For now, create a placeholder glyph run
    GlyphRun run;
    run.width = 100;
    run.height = 20;
    glyphRuns.clear();
    glyphRuns.push_back(run);
}

ImageLayoutObject::ImageLayoutObject(const MarkdownObject* sourceObject) 
    : LayoutObject(sourceObject, LayoutFlow::Block), sizeComputed(false) {
}

Size ImageLayoutObject::computeIntrinsicSize() const {
    if (!sizeComputed) {
        const_cast<ImageLayoutObject*>(this)->computeImageSize();
    }
    return intrinsicSize;
}

void ImageLayoutObject::layout(const Size& availableSpace) {
    // TODO: Layout image with proper sizing
    Size size = computeIntrinsicSize();
    setRect(Rect(0, 0, size.width, size.height));
}

void ImageLayoutObject::computeImageSize() {
    // TODO: Load and decode image to determine actual size
    intrinsicSize = Size(200, 150); // Placeholder
    sizeComputed = true;
}