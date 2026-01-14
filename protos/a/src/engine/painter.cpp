#include "painter.h"
#include "markdown_renderer.h"  // For CaretState
#include "typography.h"
#include "utf8.h"
#include <cstring>
#include <algorithm>

Painter::Painter() {
}

Painter::~Painter() {
}

PaintTree Painter::paint(const LayoutObject* layoutRoot, const CaretState* caret,
                         const char* text, int textLength) {
    // Create root artifact that will contain selection, content tree, and caret
    auto root = std::make_unique<PaintArtifact>();

    if (layoutRoot) {
        // Set root bounds to encompass the entire layout
        root->bounds = layoutRoot->getRect();

        // Paint selection first (behind text) - goes into root's display items
        if (caret && caret->hasSelection && text) {
            paintSelection(root->displayItems, *caret, text, textLength, layoutRoot);
        }

        // Paint layout tree as children - this builds the hierarchical structure
        auto contentTree = paintLayoutObject(layoutRoot);
        if (contentTree) {
            root->addChild(std::move(contentTree));
        }

        // Paint caret last (on top) - goes into root's display items
        if (caret && text) {
            paintCaret(root->displayItems, *caret, text, textLength, layoutRoot);
        }
    }

    return root;
}

std::unique_ptr<PaintArtifact> Painter::paintLayoutObject(const LayoutObject* layoutObject) {
    // Create artifact with layout object's bounds for culling
    auto artifact = std::make_unique<PaintArtifact>(layoutObject->getRect());

    // Paint background
    paintBackground(layoutObject, artifact->displayItems);

    // Draw blockquote gray bar
    if (layoutObject->getSourceObject()->getType() == MarkdownObjectType::BlockQuote) {
        paintBlockQuoteBar(layoutObject, artifact->displayItems);
    }

    // Type-specific painting
    if (const TextLayoutObject* textObject = dynamic_cast<const TextLayoutObject*>(layoutObject)) {
        paintText(textObject, artifact->displayItems);
    } else if (const ImageLayoutObject* imageObject = dynamic_cast<const ImageLayoutObject*>(layoutObject)) {
        paintImage(imageObject, artifact->displayItems);
    } else if (const TableLayoutObject* tableObject = dynamic_cast<const TableLayoutObject*>(layoutObject)) {
        paintTable(tableObject, artifact->displayItems);
    } else if (const TableRowLayoutObject* rowObject = dynamic_cast<const TableRowLayoutObject*>(layoutObject)) {
        paintTableRow(rowObject, artifact->displayItems);
    } else if (const TableCellLayoutObject* cellObject = dynamic_cast<const TableCellLayoutObject*>(layoutObject)) {
        paintTableCell(cellObject, artifact->displayItems);
    } else if (const ListItemLayoutObject* listItemObject = dynamic_cast<const ListItemLayoutObject*>(layoutObject)) {
        paintListItem(listItemObject, artifact->displayItems);
    } else if (layoutObject->getSourceObject()->getType() == MarkdownObjectType::ThematicBreak) {
        paintThematicBreak(layoutObject, artifact->displayItems);
    }

    // Paint borders
    paintBorder(layoutObject, artifact->displayItems);

    // Debug borders when DEBUG=1
    const char* debugEnv = std::getenv("DEBUG");
    if (debugEnv && strcmp(debugEnv, "1") == 0) {
        paintDebugBorder(layoutObject, artifact->displayItems);
    }

    // Recursively create child artifacts
    for (const auto& child : layoutObject->getChildren()) {
        auto childArtifact = paintLayoutObject(child.get());
        if (childArtifact) {
            artifact->addChild(std::move(childArtifact));
        }
    }

    return artifact;
}

void Painter::paintText(const TextLayoutObject* textObject, DisplayList& displayList) {
    const Rect& rect = textObject->getRect();
    Color defaultColor = getTextColor(textObject->getSourceObject());
    Color linkColor(0, 102, 204, 255);  // Blue for links
    float fontSize = textObject->getFontSize();  // P1-5: Now cached
    std::string fullText = textObject->getSourceObject()->getText();
    const auto& lines = textObject->getLines();
    bool isMonospace = textObject->getMonospace();

    if (lines.empty()) {
        // Fallback: render full text as single line
        Point textPos(rect.position.x, rect.position.y + fontSize);
        auto textOp = std::make_unique<DrawTextOp>(textPos, fullText, defaultColor, fontSize, TextStyle::Normal, isMonospace);
        displayList.push_back(std::move(textOp));
        return;
    }

    // P1-5: Use pre-computed character styles from layout phase
    // These are computed once during layout, not every paint call
    const auto& computedStyles = textObject->getComputedCharStyles();
    const auto& charStyles = computedStyles.charStyles;
    const auto& charLinkIdx = computedStyles.charLinkIdx;

    // Use actual vector sizes for bounds checking (vectors may be empty if layout not called)
    int styleLen = static_cast<int>(charStyles.size());
    int linkLen = static_cast<int>(charLinkIdx.size());

    // Render each wrapped line
    for (const auto& line : lines) {
        float lineX = rect.position.x;
        float lineY = rect.position.y + line.yOffset + fontSize;
        int charPos = line.startChar;

        while (charPos < line.endChar) {
            // Get current styling info with O(1) lookup (safe bounds check)
            int linkIdx = (charPos >= 0 && charPos < linkLen) ? charLinkIdx[charPos] : -1;
            TextStyle currentStyle = (charPos >= 0 && charPos < styleLen) ? charStyles[charPos] : TextStyle::Normal;
            bool inLink = (linkIdx >= 0);

            // Find where this segment ends (when style or link status changes)
            int segmentEnd = charPos + 1;
            while (segmentEnd < line.endChar) {
                int nextLinkIdx = (segmentEnd >= 0 && segmentEnd < linkLen) ? charLinkIdx[segmentEnd] : -1;
                TextStyle nextStyle = (segmentEnd >= 0 && segmentEnd < styleLen) ? charStyles[segmentEnd] : TextStyle::Normal;
                if (nextLinkIdx != linkIdx || nextStyle != currentStyle) {
                    break;
                }
                segmentEnd++;
            }

            // Extract segment text using character indices (not byte indices)
            std::string segmentText = utf8::substr(fullText, charPos, segmentEnd - charPos);

            // Calculate x position for this segment
            float segmentX = lineX;
            float segmentStartOffset = 0.0f;
            if (charPos > line.startChar) {
                segmentStartOffset = textObject->getCharXOffsetInLine(charPos);
                segmentX += segmentStartOffset;
            }

            // Draw text segment with appropriate color and style
            Color segmentColor = inLink ? linkColor : defaultColor;
            bool useMonospace = isMonospace || hasStyle(currentStyle, TextStyle::Code);

            // Inline code gets a slightly different color
            if (hasStyle(currentStyle, TextStyle::Code) && !inLink) {
                segmentColor = Color(200, 50, 50, 255);  // Reddish for inline code
            }

            // P1-6: Create ShapedTextRun with pre-decoded codepoints and positions
            ShapedTextRun shaped;
            shaped.originalText = segmentText;
            shaped.lineHeight = fontSize * Typography::LINE_HEIGHT_RATIO;

            // Decode codepoints and compute relative positions
            std::vector<uint32_t> decodedCodepoints = utf8::decode(segmentText);
            shaped.codepoints = std::move(decodedCodepoints);

            // Calculate x positions relative to segment start
            float prevX = 0.0f;
            for (int i = charPos; i < segmentEnd; i++) {
                float charX = textObject->getCharXOffsetInLine(i + 1) - segmentStartOffset;
                shaped.xPositions.push_back(prevX);
                prevX = charX;
            }
            shaped.totalWidth = prevX;

            Point textPos(segmentX, lineY);
            auto textOp = std::make_unique<DrawTextOp>(textPos, shaped, segmentColor, fontSize, currentStyle, useMonospace);
            displayList.push_back(std::move(textOp));

            // Draw underline for links
            if (inLink) {
                float segmentWidth = textObject->getCharXOffsetInLine(segmentEnd) -
                                     (charPos > line.startChar ? textObject->getCharXOffsetInLine(charPos) : 0);
                float underlineY = lineY + 2.0f;
                auto lineOp = std::make_unique<DrawLineOp>(
                    Point(segmentX, underlineY),
                    Point(segmentX + segmentWidth, underlineY),
                    1.0f, linkColor
                );
                displayList.push_back(std::move(lineOp));
            }

            charPos = segmentEnd;
        }
    }
}

void Painter::paintImage(const ImageLayoutObject* imageObject, DisplayList& displayList) {
    const Rect& rect = imageObject->getRect();
    const ImageObject* imgObj = static_cast<const ImageObject*>(imageObject->getSourceObject());
    auto imageOp = std::make_unique<DrawImageOp>(rect, imgObj->getSrc());
    displayList.push_back(std::move(imageOp));
}

void Painter::paintTable(const TableLayoutObject* tableObject, DisplayList& displayList) {
    const Rect& rect = tableObject->getRect();
    const std::vector<float>& columnWidths = tableObject->getColumnWidths();

    Color borderColor(200, 200, 200, 255);  // Light gray borders
    float borderWidth = 1.0f;

    // Draw outer border
    auto topBorder = std::make_unique<DrawLineOp>(
        Point(rect.position.x, rect.position.y),
        Point(rect.position.x + rect.size.width, rect.position.y),
        borderWidth, borderColor);
    displayList.push_back(std::move(topBorder));

    auto bottomBorder = std::make_unique<DrawLineOp>(
        Point(rect.position.x, rect.position.y + rect.size.height),
        Point(rect.position.x + rect.size.width, rect.position.y + rect.size.height),
        borderWidth, borderColor);
    displayList.push_back(std::move(bottomBorder));

    auto leftBorder = std::make_unique<DrawLineOp>(
        Point(rect.position.x, rect.position.y),
        Point(rect.position.x, rect.position.y + rect.size.height),
        borderWidth, borderColor);
    displayList.push_back(std::move(leftBorder));

    auto rightBorder = std::make_unique<DrawLineOp>(
        Point(rect.position.x + rect.size.width, rect.position.y),
        Point(rect.position.x + rect.size.width, rect.position.y + rect.size.height),
        borderWidth, borderColor);
    displayList.push_back(std::move(rightBorder));

    // Draw vertical column separators
    if (columnWidths.size() > 1) {
        float x = rect.position.x + borderWidth;
        for (size_t i = 0; i < columnWidths.size() - 1; i++) {
            x += columnWidths[i];
            auto colBorder = std::make_unique<DrawLineOp>(
                Point(x, rect.position.y),
                Point(x, rect.position.y + rect.size.height),
                borderWidth, borderColor);
            displayList.push_back(std::move(colBorder));
            x += borderWidth;
        }
    }
}

void Painter::paintTableRow(const TableRowLayoutObject* rowObject, DisplayList& displayList) {
    const Rect& rect = rowObject->getRect();

    // Draw header background
    if (rowObject->isHeader()) {
        Color headerBgColor(245, 245, 245, 255);  // Light gray background
        auto bgRect = std::make_unique<DrawRectOp>(rect, headerBgColor);
        displayList.push_back(std::move(bgRect));
    }

    // Draw bottom border (row separator)
    Color borderColor(200, 200, 200, 255);
    auto bottomBorder = std::make_unique<DrawLineOp>(
        Point(rect.position.x, rect.position.y + rect.size.height),
        Point(rect.position.x + rect.size.width, rect.position.y + rect.size.height),
        1.0f, borderColor);
    displayList.push_back(std::move(bottomBorder));
}

void Painter::paintTableCell(const TableCellLayoutObject* cellObject, DisplayList& displayList) {
    // Cell content is painted via children (text nodes)
    // This function could be used for cell-specific styling if needed
    (void)cellObject;
    (void)displayList;
}

void Painter::paintListItem(const ListItemLayoutObject* listItemObject, DisplayList& displayList) {
    const Rect& rect = listItemObject->getRect();
    ListMarkerType markerType = listItemObject->getMarkerType();
    const std::string& markerText = listItemObject->getMarkerText();
    int indentLevel = listItemObject->getIndentLevel();

    // Get the first child's position for accurate marker alignment
    float textY = rect.position.y;
    const auto& children = listItemObject->getChildren();
    if (!children.empty()) {
        textY = children[0]->getRect().position.y;
    }

    // Calculate marker position
    float textIndent = Typography::LIST_INDENT * (indentLevel + 1);
    float markerX = rect.position.x + textIndent - Typography::LIST_INDENT + 2.0f;
    float markerY = textY + Typography::BASE_FONT_SIZE;  // Baseline aligned with text

    Color markerColor(60, 60, 60, 255);

    if (markerType == ListMarkerType::Bullet) {
        // Draw bullet as a small filled circle
        float bulletSize = 6.0f;
        float bulletY = textY + Typography::BASE_FONT_SIZE * 0.35f;
        auto bulletOp = std::make_unique<DrawRectOp>(
            Rect(markerX + 2.0f, bulletY, bulletSize, bulletSize),
            markerColor
        );
        displayList.push_back(std::move(bulletOp));
    } else {
        // Draw the marker text (e.g., "1.", "a.")
        auto textOp = std::make_unique<DrawTextOp>(
            Point(markerX, markerY),
            markerText,
            markerColor,
            Typography::BASE_FONT_SIZE,
            TextStyle::Normal,
            false
        );
        displayList.push_back(std::move(textOp));
    }
}

void Painter::paintThematicBreak(const LayoutObject* layoutObject, DisplayList& displayList) {
    const Rect& rect = layoutObject->getRect();

    // Draw a horizontal line centered in the available height
    Color lineColor(200, 200, 200, 255);  // Gray, matching blockquote bar
    float lineThickness = 1.0f;
    float lineY = rect.position.y + rect.size.height / 2;
    float lineStartX = rect.position.x;
    float lineEndX = rect.position.x + rect.size.width;

    auto lineOp = std::make_unique<DrawLineOp>(
        Point(lineStartX, lineY),
        Point(lineEndX, lineY),
        lineThickness,
        lineColor
    );
    displayList.push_back(std::move(lineOp));
}

void Painter::paintBackground(const LayoutObject* layoutObject, DisplayList& displayList) {
    Color bgColor = getBackgroundColor(layoutObject->getSourceObject());
    if (bgColor.a > 0) {
        const Rect& rect = layoutObject->getRect();
        auto rectOp = std::make_unique<DrawRectOp>(rect, bgColor);
        displayList.push_back(std::move(rectOp));
    }
}

void Painter::paintBorder(const LayoutObject* layoutObject, DisplayList& displayList) {
    (void)layoutObject;
    (void)displayList;
    // Border painting not yet implemented
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
    switch (object->getType()) {
        case MarkdownObjectType::CodeBlock:
            return Color(240, 240, 240, 255); // Light gray
        case MarkdownObjectType::Frontmatter:
            return Color(255, 250, 230, 255); // Warm cream/pale yellow
        case MarkdownObjectType::BlockQuote:
            return Color(250, 250, 250, 255); // Very light gray
        default:
            return Color(0, 0, 0, 0); // Transparent
    }
}

void Painter::paintDebugBorder(const LayoutObject* layoutObject, DisplayList& displayList) {
    const Rect& rect = layoutObject->getRect();
    Color debugColor = Color(255, 0, 255, 255);
    auto debugOp = std::make_unique<DrawRectOp>(rect, debugColor, RectRole::Debug);
    displayList.push_back(std::move(debugOp));
}

float Painter::computeXForOffset(int offset, const char* text, float fontSize, float startX) {
    // Simple monospace approximation: each char is ~0.6 * fontSize wide
    float charWidth = fontSize * 0.6f;
    float x = startX;

    // Find the start of the line containing offset
    int lineStart = offset;
    while (lineStart > 0 && text[lineStart - 1] != '\n') {
        lineStart--;
    }

    // Count characters from line start to offset
    for (int i = lineStart; i < offset && text[i] != '\0' && text[i] != '\n'; i++) {
        x += charWidth;
    }
    return x;
}

void Painter::paintCaret(DisplayList& displayList, const CaretState& caret,
                         const char* text, int textLength, const LayoutObject* layoutRoot) {
    (void)text;
    (void)textLength;

    // Always generate caret op - visibility is handled by rasterizer

    // Find layout object for cursor position
    DOMPositionResult result = findLayoutForPosition(layoutRoot, caret.cursorPosition);
    if (!result.layout) return;

    const Rect& rect = result.layout->getRect();
    float caretX, caretY, caretHeight;

    if (result.layout->isAtomic()) {
        // Atomic element (image, etc.)
        caretY = rect.position.y;
        caretHeight = rect.size.height;
        if (result.localOffset == 0) {
            caretX = rect.position.x;  // Left edge
        } else {
            caretX = rect.position.x + rect.size.width;  // Right edge
        }
    } else if (const auto* textLayout = dynamic_cast<const TextLayoutObject*>(result.layout)) {
        // Text element - use glyph positions and line info
        caretHeight = textLayout->getFontSize();
        caretX = rect.position.x;

        const auto& lines = textLayout->getLines();
        if (!lines.empty() && result.localOffset >= 0) {
            int lineIdx = textLayout->getLineForChar(result.localOffset);
            const auto& line = lines[lineIdx];
            caretY = rect.position.y + line.yOffset;

            // Get x offset within line
            if (result.localOffset > line.startChar) {
                caretX += textLayout->getCharXOffsetInLine(result.localOffset);
            }
        } else {
            caretY = rect.position.y;
            if (result.localOffset > 0 && result.localOffset <= textLayout->getCharCount()) {
                caretX += textLayout->getCharXOffset(result.localOffset - 1);
            }
        }
    } else {
        return;  // Container or unknown type
    }

    // Use animated position if enabled
    if (caret.useAnimatedPosition) {
        caretX = caret.animatedCaretX;
        caretY = caret.animatedCaretY;
    }
    Color caretColor(0, 0, 0, 255);
    // Create a thin vertical rect for the caret (2px wide)
    Rect caretRect(caretX, caretY, 2.0f, caretHeight);
    auto caretOp = std::make_unique<DrawRectOp>(caretRect, caretColor, RectRole::Caret);
    displayList.push_back(std::move(caretOp));
}

void Painter::paintSelection(DisplayList& displayList, const CaretState& caret,
                             const char* text, int textLength, const LayoutObject* layoutRoot) {
    (void)text;
    (void)textLength;

    int startPos = std::min(caret.selectionStart, caret.selectionEnd);
    int endPos = std::max(caret.selectionStart, caret.selectionEnd);
    if (startPos == endPos) return;

    std::vector<const LayoutObject*> contentLayouts;
    collectContentLayouts(layoutRoot, contentLayouts);

    int currentPos = 0;
    Color selColor(173, 214, 255, 180);

    for (const auto* layout : contentLayouts) {
        int len = layout->getDOMLength();
        int objStart = currentPos;
        int objEnd = currentPos + len;

        // Check if selection overlaps this object
        if (startPos < objEnd && endPos > objStart) {
            int localStart = std::max(0, startPos - objStart);
            int localEnd = std::min(len, endPos - objStart);

            // Handle wrapped text with multiple lines
            if (const auto* textLayout = dynamic_cast<const TextLayoutObject*>(layout)) {
                const auto& lines = textLayout->getLines();
                if (!lines.empty()) {
                    const Rect& rect = layout->getRect();
                    float fontSize = textLayout->getFontSize();
                    float lineHeight = fontSize * Typography::LINE_HEIGHT_RATIO;

                    int startLine = textLayout->getLineForChar(localStart);
                    int endLine = textLayout->getLineForChar(localEnd > 0 ? localEnd - 1 : 0);

                    for (int lineIdx = startLine; lineIdx <= endLine; lineIdx++) {
                        const auto& line = lines[lineIdx];
                        float x1 = rect.position.x;
                        float x2 = rect.position.x + line.width;

                        // Adjust x1 for first line of selection
                        if (lineIdx == startLine && localStart > line.startChar) {
                            x1 += textLayout->getCharXOffsetInLine(localStart);
                        }

                        // Adjust x2 for last line of selection
                        if (lineIdx == endLine && localEnd < line.endChar) {
                            x2 = rect.position.x + textLayout->getCharXOffsetInLine(localEnd);
                        }

                        Rect selRect(x1, rect.position.y + line.yOffset, x2 - x1, lineHeight);
                        auto selOp = std::make_unique<DrawRectOp>(selRect, selColor, RectRole::Selection);
                        displayList.push_back(std::move(selOp));
                    }
                    currentPos += len;
                    continue;
                }
            }

            // Fallback for non-wrapped or non-text objects
            Rect selRect = computeSelectionRect(layout, localStart, localEnd);
            auto selOp = std::make_unique<DrawRectOp>(selRect, selColor, RectRole::Selection);
            displayList.push_back(std::move(selOp));
        }
        currentPos += len;
    }
}

// Collect all content-bearing layout objects in document order
void Painter::collectContentLayouts(const LayoutObject* obj, std::vector<const LayoutObject*>& out) {
    if (!obj) return;

    // Check if this object has DOM content
    if (obj->getDOMLength() > 0) {
        out.push_back(obj);
    }

    // Recurse into children
    for (const auto& child : obj->getChildren()) {
        collectContentLayouts(child.get(), out);
    }
}

// Find layout object and local offset for a global DOM position
DOMPositionResult Painter::findLayoutForPosition(const LayoutObject* root, int domPosition) {
    DOMPositionResult result;

    std::vector<const LayoutObject*> contentLayouts;
    collectContentLayouts(root, contentLayouts);

    int currentPos = 0;
    for (const auto* layout : contentLayouts) {
        int len = layout->getDOMLength();
        if (domPosition <= currentPos + len) {
            // Found the containing object
            result.layout = layout;
            result.localOffset = domPosition - currentPos;
            result.isAtomicBoundary = layout->isAtomic();
            return result;
        }
        currentPos += len;
    }

    // Position is at or beyond end - use last object
    if (!contentLayouts.empty()) {
        result.layout = contentLayouts.back();
        result.localOffset = result.layout->getDOMLength();
        result.isAtomicBoundary = result.layout->isAtomic();
    }

    return result;
}

void Painter::paintBlockQuoteBar(const LayoutObject* layoutObject, DisplayList& displayList) {
    const Rect& rect = layoutObject->getRect();
    Color barColor(200, 200, 200, 255);  // Gray

    // Draw vertical bar on the left side
    float barX = rect.position.x - 15.0f;
    float barTop = rect.position.y;
    float barBottom = rect.position.y + rect.size.height - 10.0f;  // Slight offset

    auto lineOp = std::make_unique<DrawLineOp>(
        Point(barX, barTop),
        Point(barX, barBottom),
        3.0f,  // thickness
        barColor
    );
    displayList.push_back(std::move(lineOp));
}

void Painter::paintLinkUnderline(const TextLayoutObject* textObject, DisplayList& displayList) {
    const Rect& rect = textObject->getRect();
    float fontSize = textObject->getFontSize();
    Color linkColor(0, 102, 204, 255);  // Blue

    const auto& lines = textObject->getLines();
    if (lines.empty()) {
        // Single line fallback
        float y = rect.position.y + fontSize + 2.0f;  // Below baseline
        float width = rect.size.width;
        auto lineOp = std::make_unique<DrawLineOp>(
            Point(rect.position.x, y),
            Point(rect.position.x + width, y),
            1.0f,
            linkColor
        );
        displayList.push_back(std::move(lineOp));
        return;
    }

    // Draw underline for each line
    for (const auto& line : lines) {
        float y = rect.position.y + line.yOffset + fontSize + 2.0f;
        auto lineOp = std::make_unique<DrawLineOp>(
            Point(rect.position.x, y),
            Point(rect.position.x + line.width, y),
            1.0f,
            linkColor
        );
        displayList.push_back(std::move(lineOp));
    }
}

bool Painter::isInsideLink(const LayoutObject* layoutObject) {
    // Check if this layout object or any ancestor is a link
    const LayoutObject* current = layoutObject;
    while (current) {
        if (current->getSourceObject()->getType() == MarkdownObjectType::Link) {
            return true;
        }
        current = current->getParent();
    }
    return false;
}

// Compute selection rect for a portion of a layout object
// Note: For wrapped text, this returns the rect for the first line only.
// Multi-line selection needs multiple rects - handled in paintSelection
Rect Painter::computeSelectionRect(const LayoutObject* layout, int localStart, int localEnd) {
    const Rect& rect = layout->getRect();

    if (layout->isAtomic()) {
        // Atomic element: select entire rect
        return rect;
    }

    if (const auto* textLayout = dynamic_cast<const TextLayoutObject*>(layout)) {
        const auto& lines = textLayout->getLines();
        float fontSize = textLayout->getFontSize();
        float lineHeight = fontSize * Typography::LINE_HEIGHT_RATIO;

        if (lines.empty()) {
            // Fallback for unwrapped text
            float x1 = rect.position.x;
            float x2 = rect.position.x;

            if (localStart > 0 && localStart <= textLayout->getCharCount()) {
                x1 += textLayout->getCharXOffset(localStart - 1);
            }
            if (localEnd > 0 && localEnd <= textLayout->getCharCount()) {
                x2 += textLayout->getCharXOffset(localEnd - 1);
            } else if (localEnd > textLayout->getCharCount() && textLayout->getCharCount() > 0) {
                x2 += textLayout->getCharXOffset(textLayout->getCharCount() - 1);
            }

            return Rect(x1, rect.position.y, x2 - x1, lineHeight);
        }

        // Find which line the selection starts on
        int startLine = textLayout->getLineForChar(localStart);
        float x1 = rect.position.x;
        if (localStart > lines[startLine].startChar) {
            x1 += textLayout->getCharXOffsetInLine(localStart);
        }

        // For single-line selection within one line
        int endLine = textLayout->getLineForChar(localEnd > 0 ? localEnd - 1 : 0);
        if (startLine == endLine) {
            float x2 = rect.position.x;
            if (localEnd > lines[startLine].startChar) {
                x2 += textLayout->getCharXOffsetInLine(localEnd);
            }
            return Rect(x1, rect.position.y + lines[startLine].yOffset, x2 - x1, lineHeight);
        }

        // Multi-line: return first line rect (paintSelection handles the rest)
        float x2 = rect.position.x + lines[startLine].width;
        return Rect(x1, rect.position.y + lines[startLine].yOffset, x2 - x1, lineHeight);
    }

    return rect;
}