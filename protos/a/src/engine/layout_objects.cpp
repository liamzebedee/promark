#include "layout_objects.h"
#include "typography.h"
#include "utf8.h"
#include <jpeglib.h>
#include "stb/stb_image.h"

LayoutObject::LayoutObject(const MarkdownObject* sourceObject, LayoutFlow flow)
    : sourceObject(sourceObject), flow(flow), parent(nullptr),
      cachedFontSize(-1.0f), cachedFontSizeParent(nullptr) {
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
    // P1-5: Use cached font size if valid
    if (cachedFontSize > 0 && cachedFontSizeParent == parent) {
        return cachedFontSize;
    }

    // Compute and cache font size
    switch (sourceObject->getType()) {
        case MarkdownObjectType::Heading: {
            const HeadingObject* heading = static_cast<const HeadingObject*>(sourceObject);
            cachedFontSize = Typography::headingSize(heading->getLevel());
            break;
        }
        case MarkdownObjectType::Text:
            // Text inherits font size from parent
            if (parent) {
                cachedFontSize = parent->getFontSize();
            } else {
                cachedFontSize = Typography::BASE_FONT_SIZE;
            }
            break;
        default:
            cachedFontSize = Typography::BASE_FONT_SIZE;
    }

    cachedFontSizeParent = parent;
    return cachedFontSize;
}

void LayoutObject::invalidateFontSizeCache() {
    cachedFontSize = -1.0f;
    cachedFontSizeParent = nullptr;
    // Also invalidate children's caches since they may inherit from parent
    for (const auto& child : children) {
        child->invalidateFontSizeCache();
    }
}

void LayoutObject::setParent(LayoutObject* parent) {
    // P1-5: Just set parent - getFontSize() will detect the change via cachedFontSizeParent check
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


TextLayoutObject::TextLayoutObject(const MarkdownObject* sourceObject)
    : LayoutObject(sourceObject, LayoutFlow::Block), fontProvider(nullptr), availableWidth(0) {
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

    // Compute width using FontProvider or fallback estimate
    float width = 0.0f;
    if (fontProvider) {
        size_t pos = 0;
        while (pos < text.length()) {
            uint32_t codepoint = utf8::decode(text, pos);
            width += fontProvider->getGlyphAdvance(codepoint, fontSize, isMonospace);
        }
    } else {
        width = utf8::length(text) * fontSize * 0.6f;
    }

    return Size(width, fontSize);
}

float TextLayoutObject::getFontSize() const {
    // Text objects inherit font size from their parent layout object
    if (parent) {
        return parent->getFontSize();
    }
    return Typography::BASE_FONT_SIZE;
}

void TextLayoutObject::layout(const Size& availableSpace) {
    availableWidth = availableSpace.width;
    shapeText();
    wrapText(availableSpace.width);
    computeCharStyles();  // P1-5: Pre-compute styles for painter
    Size textSize = computeIntrinsicSize();
    setRect(Rect(0, 0, availableSpace.width, textSize.height));
}

const std::vector<TextLayoutObject::GlyphRun>& TextLayoutObject::getGlyphRuns() const {
    return glyphRuns;
}

void TextLayoutObject::setFontProvider(const FontProvider* provider) {
    fontProvider = provider;
}

int TextLayoutObject::getDOMLength() const {
    // Return code point count, not byte count
    int len = static_cast<int>(utf8::length(sourceObject->getText()));
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

    // Get style ranges and pre-compute code style flags for O(1) lookup
    const auto& styleRanges = getStyleRanges();
    size_t textCodepoints = utf8::length(text);
    std::vector<bool> isCodeStyle(textCodepoints, false);

    // Mark code-styled characters (O(m) where m = number of style ranges)
    for (const auto& sr : styleRanges) {
        if (hasStyle(sr.style, TextStyle::Code)) {
            for (int i = sr.startChar; i < sr.endChar && i < (int)textCodepoints; i++) {
                if (i >= 0) isCodeStyle[i] = true;
            }
        }
    }

    if (fontProvider) {
        // Use FontProvider for accurate glyph metrics with UTF-8 decoding
        size_t pos = 0;
        int charIdx = 0;
        while (pos < text.length()) {
            uint32_t codepoint = utf8::decode(text, pos);

            // Newline and other control characters have zero width
            if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') {
                charXOffsets.push_back(x);
                charIdx++;
                continue;
            }

            // Use monospace font if this is a code block or inline code character
            bool useMono = isMonospace || (charIdx < (int)isCodeStyle.size() && isCodeStyle[charIdx]);
            x += fontProvider->getGlyphAdvance(codepoint, fontSize, useMono);
            charXOffsets.push_back(x);
            charIdx++;
        }
    } else {
        // Fallback: monospace approximation (still need to decode UTF-8)
        float charWidth = fontSize * 0.6f;
        size_t pos = 0;
        while (pos < text.length()) {
            uint32_t codepoint = utf8::decode(text, pos);
            // Newline and control characters have zero width
            if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') {
                charXOffsets.push_back(x);
                continue;
            }
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

    // Decode UTF-8 to code points for iteration
    std::vector<uint32_t> codepoints = utf8::decode(text);
    size_t numCodepoints = codepoints.size();

    for (size_t i = 0; i < numCodepoints; i++) {
        uint32_t cp = codepoints[i];
        float charEndX = charXOffsets[i];
        float lineWidth = charEndX - lineStartX;

        // Force line break on newline character
        if (cp == '\n') {
            LineInfo line;
            line.startChar = lineStart;
            line.endChar = i;  // Don't include the newline
            line.yOffset = lines.size() * lineHeight;
            line.width = (i > 0 && lineStart < (int)i) ?
                (charXOffsets[i - 1] - lineStartX) : 0;
            lines.push_back(line);

            // Start new line after the newline
            lineStart = i + 1;
            lineStartX = charEndX;
            lastWordEnd = lineStart;
            lastWordEndX = lineStartX;
            continue;
        }

        // Track word boundaries (space ends a word)
        if (cp == ' ') {
            lastWordEnd = i;
            lastWordEndX = charEndX;
        }

        // Check if we need to wrap due to width
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

    // Add final line (if there's content after the last newline)
    if (lineStart <= (int)numCodepoints) {
        LineInfo line;
        line.startChar = lineStart;
        line.endChar = numCodepoints;
        line.yOffset = lines.size() * lineHeight;
        line.width = (lineStart < (int)charXOffsets.size()) ?
            (charXOffsets.back() - lineStartX) : 0;
        lines.push_back(line);
    }
}

const std::vector<TextLayoutObject::LineInfo>& TextLayoutObject::getLines() const {
    return lines;
}

int TextLayoutObject::getLineForChar(int charIndex) const {
    for (size_t i = 0; i < lines.size(); i++) {
        // Use <= for endChar to handle positions at line boundaries correctly
        // Position at endChar belongs to this line, not the next
        if (charIndex >= lines[i].startChar && charIndex <= lines[i].endChar) {
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

const std::vector<InlineLinkRange>& TextLayoutObject::getLinkRanges() const {
    // Get link ranges from parent paragraph
    if (parent && parent->getSourceObject()) {
        return parent->getSourceObject()->getLinkRanges();
    }
    static std::vector<InlineLinkRange> empty;
    return empty;
}

const std::vector<InlineStyleRange>& TextLayoutObject::getStyleRanges() const {
    // Get style ranges from parent paragraph
    if (parent && parent->getSourceObject()) {
        return parent->getSourceObject()->getStyleRanges();
    }
    static std::vector<InlineStyleRange> empty;
    return empty;
}

// P1-5: Pre-compute character-level styles during layout
// This is done once per layout pass, not every paint call
void TextLayoutObject::computeCharStyles() {
    const std::string& fullText = sourceObject->getText();
    int charCount = static_cast<int>(utf8::length(fullText));

    const auto& styleRanges = getStyleRanges();
    const auto& linkRanges = getLinkRanges();

    // Check if recomputation is needed
    if (computedCharStyles.cachedCharCount == charCount &&
        computedCharStyles.cachedStyleRangeCount == styleRanges.size() &&
        computedCharStyles.cachedLinkRangeCount == linkRanges.size()) {
        return;  // Cache is still valid
    }

    // Clear and resize
    computedCharStyles.charStyles.assign(charCount, TextStyle::Normal);
    computedCharStyles.charLinkIdx.assign(charCount, -1);
    computedCharStyles.isCodeStyle.assign(charCount, false);

    // Fill in styles (O(m*n) where m = style ranges, n = avg range size)
    for (const auto& sr : styleRanges) {
        for (int i = sr.startChar; i < sr.endChar && i < charCount; i++) {
            if (i >= 0) {
                computedCharStyles.charStyles[i] = sr.style;
                if (hasStyle(sr.style, TextStyle::Code)) {
                    computedCharStyles.isCodeStyle[i] = true;
                }
            }
        }
    }

    // Fill in link indices
    for (size_t li = 0; li < linkRanges.size(); li++) {
        const auto& lr = linkRanges[li];
        for (int i = lr.startChar; i < lr.endChar && i < charCount; i++) {
            if (i >= 0) {
                computedCharStyles.charLinkIdx[i] = static_cast<int>(li);
            }
        }
    }

    // Update validation metadata
    computedCharStyles.cachedCharCount = charCount;
    computedCharStyles.cachedStyleRangeCount = styleRanges.size();
    computedCharStyles.cachedLinkRangeCount = linkRanges.size();
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
        // File path - use stb_image to get dimensions
        int width, height, channels;
        if (stbi_info(src.c_str(), &width, &height, &channels)) {
            intrinsicSize = Size(static_cast<float>(width), static_cast<float>(height));
        }
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

// Table Layout Objects

TableLayoutObject::TableLayoutObject(const MarkdownObject* sourceObject)
    : LayoutObject(sourceObject, LayoutFlow::Block) {
}

void TableLayoutObject::computeColumnWidths(float availableWidth) {
    const TableObject* tableObj = static_cast<const TableObject*>(sourceObject);
    int columnCount = tableObj->getColumnCount();
    if (columnCount == 0) return;

    // Simple equal-width columns
    float borderWidth = 1.0f;
    float totalBorders = (columnCount + 1) * borderWidth;
    float usableWidth = availableWidth - totalBorders;
    float columnWidth = usableWidth / columnCount;

    columnWidths.clear();
    for (int i = 0; i < columnCount; i++) {
        columnWidths.push_back(columnWidth);
    }
}

void TableLayoutObject::layout(const Size& availableSpace) {
    // NOTE: This method is now bypassed - LayoutEngine::layoutTable() handles table layout.
    // The engine is the sole authority for positioning (unified layout authority).
    // This implementation is kept for backwards compatibility but should be removed in cleanup.
    computeColumnWidths(availableSpace.width);
    rect.size.width = availableSpace.width;
    rect.size.height = 0;  // Size computed by engine
}

TableRowLayoutObject::TableRowLayoutObject(const MarkdownObject* sourceObject)
    : LayoutObject(sourceObject, LayoutFlow::Block) {
}

bool TableRowLayoutObject::isHeader() const {
    const TableRowObject* rowObj = static_cast<const TableRowObject*>(sourceObject);
    return rowObj->isHeader();
}

void TableRowLayoutObject::layout(const Size& availableSpace) {
    // NOTE: This method is now bypassed - LayoutEngine::layoutTableRow() handles row layout.
    // The engine is the sole authority for positioning (unified layout authority).
    // This implementation is kept for backwards compatibility but should be removed in cleanup.
    rect.size.width = availableSpace.width;
    rect.size.height = 0;  // Size computed by engine
}

TableCellLayoutObject::TableCellLayoutObject(const MarkdownObject* sourceObject)
    : LayoutObject(sourceObject, LayoutFlow::Block) {
}

TableCellAlign TableCellLayoutObject::getAlignment() const {
    const TableCellObject* cellObj = static_cast<const TableCellObject*>(sourceObject);
    return cellObj->getAlignment();
}

void TableCellLayoutObject::layout(const Size& availableSpace) {
    // NOTE: This method is now bypassed - LayoutEngine::layoutTableCell() handles cell layout.
    // The engine is the sole authority for positioning (unified layout authority).
    // This implementation is kept for backwards compatibility but should be removed in cleanup.
    rect.size.width = availableSpace.width;
    rect.size.height = 0;  // Size computed by engine
}

// List Layout Objects

ListItemLayoutObject::ListItemLayoutObject(const MarkdownObject* sourceObject)
    : LayoutObject(sourceObject, LayoutFlow::Block) {
}

void ListItemLayoutObject::layout(const Size& availableSpace) {
    // NOTE: This method is now bypassed - LayoutEngine::layoutListItem() handles list item layout.
    // The engine is the sole authority for positioning (unified layout authority).
    // This implementation is kept for backwards compatibility but should be removed in cleanup.
    rect.size.width = availableSpace.width;
    rect.size.height = Typography::BASE_FONT_SIZE;  // Minimum height
}

ListMarkerType ListItemLayoutObject::getMarkerType() const {
    const ListItemObject* itemObj = static_cast<const ListItemObject*>(sourceObject);
    return itemObj->getMarkerType();
}

const std::string& ListItemLayoutObject::getMarkerText() const {
    const ListItemObject* itemObj = static_cast<const ListItemObject*>(sourceObject);
    return itemObj->getMarkerText();
}

int ListItemLayoutObject::getIndentLevel() const {
    const ListItemObject* itemObj = static_cast<const ListItemObject*>(sourceObject);
    return itemObj->getIndentLevel();
}