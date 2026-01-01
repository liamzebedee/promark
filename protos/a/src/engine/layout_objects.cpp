#include "layout_objects.h"
#include <jpeglib.h>

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
    // Base size: 14pt ≈ 28px on Retina (2x DPI)
    constexpr float baseFontSize = 28.0f;

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
    : LayoutObject(sourceObject, LayoutFlow::Inline), fontFace(nullptr), availableWidth(0) {
}

Size TextLayoutObject::computeIntrinsicSize() const {
    std::string text = sourceObject->getText();
    float fontSize = getFontSize();
    float lineHeight = fontSize;

    if (text.empty()) {
        return Size(0, lineHeight);
    }

    // If we have lines computed, use them for size
    if (!lines.empty()) {
        float maxWidth = 0;
        for (const auto& line : lines) {
            maxWidth = std::max(maxWidth, line.width);
        }
        return Size(maxWidth, lines.size() * lineHeight);
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

    return Size(width, fontSize);
}

float TextLayoutObject::getFontSize() const {
    // Text objects inherit font size from their parent layout object
    if (parent) {
        return parent->getFontSize();
    }
    return 16.0f; // Default body text
}

void TextLayoutObject::layout(const Size& availableSpace) {
    availableWidth = availableSpace.width;
    shapeText();
    wrapText(availableSpace.width);
    Size textSize = computeIntrinsicSize();
    setRect(Rect(0, 0, availableSpace.width, textSize.height));
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

void TextLayoutObject::wrapText(float maxWidth) {
    lines.clear();
    std::string text = sourceObject->getText();

    if (text.empty() || charXOffsets.empty()) {
        // Empty line still needs a line entry
        LineInfo line;
        line.startChar = 0;
        line.endChar = 0;
        line.yOffset = 0;
        line.width = 0;
        lines.push_back(line);
        return;
    }

    float fontSize = getFontSize();
    float lineHeight = fontSize;
    int lineStart = 0;
    float lineStartX = 0;
    int lastWordEnd = 0;
    float lastWordEndX = 0;

    for (size_t i = 0; i < text.length(); i++) {
        float charEndX = charXOffsets[i];
        float lineWidth = charEndX - lineStartX;

        // Track word boundaries (space ends a word)
        if (text[i] == ' ') {
            lastWordEnd = i;
            lastWordEndX = charEndX;
        }

        // Check if we need to wrap
        if (lineWidth > maxWidth && i > static_cast<size_t>(lineStart)) {
            LineInfo line;
            line.yOffset = lines.size() * lineHeight;

            // Try to break at word boundary
            if (lastWordEnd > lineStart) {
                line.startChar = lineStart;
                line.endChar = lastWordEnd;
                line.width = lastWordEndX - lineStartX;
                lineStart = lastWordEnd + 1;  // Skip the space
                lineStartX = (lineStart < (int)charXOffsets.size()) ?
                    (lineStart > 0 ? charXOffsets[lineStart - 1] : 0) : charEndX;
            } else {
                // No word boundary - break at current char
                line.startChar = lineStart;
                line.endChar = i;
                line.width = (i > 0 ? charXOffsets[i - 1] : 0) - lineStartX;
                lineStart = i;
                lineStartX = (i > 0) ? charXOffsets[i - 1] : 0;
            }
            lines.push_back(line);
            lastWordEnd = lineStart;
            lastWordEndX = lineStartX;
        }
    }

    // Add final line
    LineInfo line;
    line.startChar = lineStart;
    line.endChar = text.length();
    line.yOffset = lines.size() * lineHeight;
    line.width = charXOffsets.back() - lineStartX;
    lines.push_back(line);
}

const std::vector<TextLayoutObject::LineInfo>& TextLayoutObject::getLines() const {
    return lines;
}

int TextLayoutObject::getLineForChar(int charIndex) const {
    for (size_t i = 0; i < lines.size(); i++) {
        if (charIndex >= lines[i].startChar && charIndex < lines[i].endChar) {
            return i;
        }
    }
    // Return last line if at end
    return lines.empty() ? 0 : lines.size() - 1;
}

float TextLayoutObject::getCharXOffsetInLine(int charIndex) const {
    if (charXOffsets.empty() || lines.empty()) return 0;

    int lineIdx = getLineForChar(charIndex);
    int lineStart = lines[lineIdx].startChar;
    float lineStartX = (lineStart > 0) ? charXOffsets[lineStart - 1] : 0;

    if (charIndex <= 0) return 0;
    if (charIndex > (int)charXOffsets.size()) charIndex = charXOffsets.size();

    return charXOffsets[charIndex - 1] - lineStartX;
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

// Helper to decode base64
static bool decodeBase64(const std::string& base64, std::vector<uint8_t>& outBytes) {
    static const std::string base64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<int> lookup(256, -1);
    for (int i = 0; i < 64; i++) {
        lookup[static_cast<unsigned char>(base64Chars[i])] = i;
    }

    outBytes.clear();
    outBytes.reserve(base64.size() * 3 / 4);

    uint32_t buffer = 0;
    int bitsCollected = 0;

    for (char c : base64) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        int value = lookup[static_cast<unsigned char>(c)];
        if (value < 0) continue;

        buffer = (buffer << 6) | value;
        bitsCollected += 6;

        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            outBytes.push_back(static_cast<uint8_t>((buffer >> bitsCollected) & 0xFF));
        }
    }
    return true;
}

void ImageLayoutObject::computeImageSize() const {
    const ImageObject* imgObj = static_cast<const ImageObject*>(sourceObject);
    const std::string& src = imgObj->getSrc();

    // Default size
    intrinsicSize = Size(200, 150);
    sizeComputed = true;

    // Check for data URI
    if (src.substr(0, 5) != "data:") {
        return;
    }

    size_t commaPos = src.find(',');
    if (commaPos == std::string::npos) return;

    std::string header = src.substr(0, commaPos);
    std::string base64Data = src.substr(commaPos + 1);

    if (header.find(";base64") == std::string::npos) return;

    std::vector<uint8_t> imageBytes;
    if (!decodeBase64(base64Data, imageBytes)) return;

    // Try to get dimensions from PNG header
    if (header.find("image/png") != std::string::npos) {
        if (imageBytes.size() >= 24 &&
            imageBytes[0] == 137 && imageBytes[1] == 80 &&
            imageBytes[2] == 78 && imageBytes[3] == 71) {
            // IHDR chunk at offset 8
            uint32_t width = (imageBytes[16] << 24) | (imageBytes[17] << 16) |
                            (imageBytes[18] << 8) | imageBytes[19];
            uint32_t height = (imageBytes[20] << 24) | (imageBytes[21] << 16) |
                             (imageBytes[22] << 8) | imageBytes[23];
            intrinsicSize = Size(static_cast<float>(width), static_cast<float>(height));
        }
    }
    // Try to get dimensions from JPEG header
    else if (header.find("image/jpeg") != std::string::npos ||
             header.find("image/jpg") != std::string::npos) {
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);
        jpeg_mem_src(&cinfo, imageBytes.data(), imageBytes.size());

        if (jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK) {
            intrinsicSize = Size(static_cast<float>(cinfo.image_width),
                                 static_cast<float>(cinfo.image_height));
        }
        jpeg_destroy_decompress(&cinfo);
    }
}