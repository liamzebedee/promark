#include "painter.h"
#include <iostream>
#include <string>

Painter::Painter() {
}

Painter::~Painter() {
}

DisplayList Painter::paint(const LayoutObject* layoutRoot) {
    DisplayList displayList;
    
    // Reset static positioning for each paint cycle
    resetTextPositioning();
    
    if (layoutRoot) {
        paintLayoutObject(layoutRoot, displayList);
    }
    
    return displayList;
}

void Painter::paintLayoutObject(const LayoutObject* layoutObject, DisplayList& displayList) {
    // Paint background
    paintBackground(layoutObject, displayList);
    
    // Paint based on object type
    const MarkdownObject* sourceObject = layoutObject->getSourceObject();
    
    if (const TextLayoutObject* textObject = dynamic_cast<const TextLayoutObject*>(layoutObject)) {
        paintText(textObject, displayList);
    } else if (const ImageLayoutObject* imageObject = dynamic_cast<const ImageLayoutObject*>(layoutObject)) {
        paintImage(imageObject, displayList);
    }
    
    // Paint border
    paintBorder(layoutObject, displayList);
    
    // Debug: Paint layout rect borders if DEBUG=1
    const char* debugEnv = std::getenv("DEBUG");
    if (debugEnv && std::string(debugEnv) == "1") {
        paintDebugBorder(layoutObject, displayList);
    }
    
    // Paint children
    for (const auto& child : layoutObject->getChildren()) {
        paintLayoutObject(child.get(), displayList);
    }
}

void Painter::resetTextPositioning() {
    // No longer needed - using layout rects
}

void Painter::paintText(const TextLayoutObject* textObject, DisplayList& displayList) {
    const Rect& rect = textObject->getRect();
    Color textColor = getTextColor(textObject->getSourceObject());
    float fontSize = textObject->getFontSize();
    std::string text = textObject->getSourceObject()->getText();

    // Use position from layout rect
    // Text baseline offset: move down by fontSize since rect.y is top of text box
    Point textPos(rect.position.x, rect.position.y + fontSize);
    auto textOp = std::make_unique<DrawTextOp>(textPos, text, textColor, fontSize);
    displayList.push_back(std::move(textOp));
}

void Painter::paintImage(const ImageLayoutObject* imageObject, DisplayList& displayList) {
    // TODO: Paint image
    const Rect& rect = imageObject->getRect();
    const ImageObject* imgObj = static_cast<const ImageObject*>(imageObject->getSourceObject());
    
    auto imageOp = std::make_unique<DrawImageOp>(rect, imgObj->getSrc());
    displayList.push_back(std::move(imageOp));
}

void Painter::paintBackground(const LayoutObject* layoutObject, DisplayList& displayList) {
    // TODO: Paint background based on object type
    Color bgColor = getBackgroundColor(layoutObject->getSourceObject());
    
    // Only paint background if it's not transparent
    if (bgColor.a > 0) {
        const Rect& rect = layoutObject->getRect();
        auto rectOp = std::make_unique<DrawRectOp>(rect, bgColor);
        displayList.push_back(std::move(rectOp));
    }
}

void Painter::paintBorder(const LayoutObject* layoutObject, DisplayList& displayList) {
    // TODO: Paint borders based on object styling
}

Color Painter::getTextColor(const MarkdownObject* object) {
    switch (object->getType()) {
        case MarkdownObjectType::Heading:
            return Color(0, 0, 0, 255); // Black
        case MarkdownObjectType::Link:
            return Color(0, 0, 255, 255); // Blue
        default:
            return Color(0, 0, 0, 255); // Black
    }
}

Color Painter::getBackgroundColor(const MarkdownObject* object) {
    // TODO: Get background color based on object type
    switch (object->getType()) {
        case MarkdownObjectType::CodeBlock:
            return Color(240, 240, 240, 255); // Light gray
        case MarkdownObjectType::BlockQuote:
            return Color(250, 250, 250, 255); // Very light gray
        default:
            return Color(0, 0, 0, 0); // Transparent
    }
}

void Painter::paintDebugBorder(const LayoutObject* layoutObject, DisplayList& displayList) {
    const Rect& rect = layoutObject->getRect();
    
    // Use BRIGHT MAGENTA so it's very visible
    Color debugColor = Color(255, 0, 255, 255); // Bright magenta, fully opaque
    
    // Create debug border rect (we'll draw it as an outline in the rasterizer)
    auto debugOp = std::make_unique<DrawDebugBorderOp>(rect, debugColor);
    displayList.push_back(std::move(debugOp));
}