#include "painter.h"
#include <iostream>

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
    
    // Paint children
    for (const auto& child : layoutObject->getChildren()) {
        paintLayoutObject(child.get(), displayList);
    }
}

// Static variable for text positioning
static float nextY = 50;

void Painter::resetTextPositioning() {
    nextY = 50; // Reset to top
}

void Painter::paintText(const TextLayoutObject* textObject, DisplayList& displayList) {
    const Rect& rect = textObject->getRect();
    Color textColor = getTextColor(textObject->getSourceObject());
    float fontSize = textObject->getFontSize();
    std::string text = textObject->getSourceObject()->getText();
    
    // Check if this text is inside a heading
    bool isHeading = false;
    if (textObject->getParent() && textObject->getParent()->getSourceObject()) {
        isHeading = (textObject->getParent()->getSourceObject()->getType() == MarkdownObjectType::Heading);
    }
    
    float yPos = nextY;
    
    if (isHeading) {
        std::cout << "HEADING: " << text << " fontSize=" << fontSize << std::endl;
    } else {
        std::cout << "BODY: " << text << " fontSize=" << fontSize << std::endl;
    }
    
    // Draw text at position
    Point textPos(50, yPos);
    auto textOp = std::make_unique<DrawTextOp>(textPos, text, textColor, fontSize);
    displayList.push_back(std::move(textOp));
    
    // Move down for next element
    nextY += fontSize + 10;
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