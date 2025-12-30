#include "layout_objects.h"

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
    // Base size: 11pt ≈ 22px on Retina (2x DPI)
    constexpr float baseFontSize = 22.0f;

    switch (sourceObject->getType()) {
        case MarkdownObjectType::Heading: {
            const HeadingObject* heading = static_cast<const HeadingObject*>(sourceObject);
            switch (heading->getLevel()) {
                case 1: return baseFontSize * 2.0f;    // 44px
                case 2: return baseFontSize * 1.75f;   // 38.5px
                case 3: return baseFontSize * 1.5f;    // 33px
                case 4: return baseFontSize * 1.25f;   // 27.5px
                case 5: return baseFontSize * 1.1f;    // 24.2px
                case 6: return baseFontSize;           // 22px
                default: return baseFontSize * 1.5f;
            }
        }
        case MarkdownObjectType::Text:
            // Text inherits font size from parent
            if (parent) {
                return parent->getFontSize();
            }
            return baseFontSize;
        default:
            return baseFontSize;
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
    // Note: This is called for nested blocks. Root layout is handled by LayoutEngine.
    // For nested blocks, children are positioned relative to parent.
    float currentY = 0;

    for (const auto& child : children) {
        Size childAvailableSpace(availableSpace.width, availableSpace.height - currentY);
        child->layout(childAvailableSpace);

        const Rect& childRect = child->getRect();
        child->setRect(Rect(0, currentY, childRect.size.width, childRect.size.height));
        currentY += childRect.size.height;
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
    : LayoutObject(sourceObject, LayoutFlow::Inline), fontFace(nullptr) {
}

Size TextLayoutObject::computeIntrinsicSize() const {
    std::string text = sourceObject->getText();
    float fontSize = getFontSize();
    float lineHeight = fontSize * 1.2f;

    if (text.empty()) {
        return Size(0, lineHeight);
    }

    // If we have glyph offsets computed, use the last one for width
    if (!charXOffsets.empty()) {
        return Size(charXOffsets.back(), lineHeight);
    }

    // Fallback: compute width using FreeType or monospace estimate
    float width = 0.0f;
    if (fontFace) {
        FT_Set_Pixel_Sizes(fontFace, 0, static_cast<FT_UInt>(fontSize));
        for (char c : text) {
            FT_UInt glyphIndex = FT_Get_Char_Index(fontFace, static_cast<FT_ULong>(c));
            if (FT_Load_Glyph(fontFace, glyphIndex, FT_LOAD_DEFAULT) == 0) {
                width += fontFace->glyph->advance.x / 64.0f;
            } else {
                width += fontSize * 0.6f;
            }
        }
    } else {
        width = text.length() * fontSize * 0.6f;
    }

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

void TextLayoutObject::setFontFace(FT_Face face) {
    fontFace = face;
}

int TextLayoutObject::getDOMLength() const {
    int len = static_cast<int>(sourceObject->getText().length());
    // Empty lines still occupy 1 DOM position (like a newline)
    return (len == 0) ? 1 : len;
}

int TextLayoutObject::getCharCount() const {
    return static_cast<int>(charXOffsets.size());
}

float TextLayoutObject::getCharXOffset(int index) const {
    if (index < 0 || index >= static_cast<int>(charXOffsets.size())) {
        return 0.0f;
    }
    return charXOffsets[index];
}

void TextLayoutObject::shapeText() {
    glyphRuns.clear();
    charXOffsets.clear();

    std::string text = sourceObject->getText();
    if (text.empty()) {
        return;
    }

    float fontSize = getFontSize();
    float x = 0.0f;

    if (fontFace) {
        // Use FreeType for accurate glyph metrics
        FT_Set_Pixel_Sizes(fontFace, 0, static_cast<FT_UInt>(fontSize));

        for (size_t i = 0; i < text.length(); i++) {
            FT_UInt glyphIndex = FT_Get_Char_Index(fontFace, static_cast<FT_ULong>(text[i]));
            if (FT_Load_Glyph(fontFace, glyphIndex, FT_LOAD_DEFAULT) == 0) {
                x += fontFace->glyph->advance.x / 64.0f;  // advance in 1/64 pixels
            } else {
                // Fallback for missing glyphs
                x += fontSize * 0.6f;
            }
            charXOffsets.push_back(x);
        }
    } else {
        // Fallback: monospace approximation
        float charWidth = fontSize * 0.6f;
        for (size_t i = 0; i < text.length(); i++) {
            x += charWidth;
            charXOffsets.push_back(x);
        }
    }
}

ImageLayoutObject::ImageLayoutObject(const MarkdownObject* sourceObject) 
    : LayoutObject(sourceObject, LayoutFlow::Block), sizeComputed(false) {
}

Size ImageLayoutObject::computeIntrinsicSize() const {
    if (!sizeComputed) {
        computeImageSize();
    }
    return intrinsicSize;
}

void ImageLayoutObject::layout(const Size& availableSpace) {
    (void)availableSpace;  // Will be used for constraining image size
    Size size = computeIntrinsicSize();
    setRect(Rect(0, 0, size.width, size.height));
}

void ImageLayoutObject::computeImageSize() const {
    // Placeholder - actual image decoding not yet implemented
    intrinsicSize = Size(200, 150);
    sizeComputed = true;
}