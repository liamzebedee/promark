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

void MarkdownRenderer::setCaretState(const CaretState& state) {
    // Only repaint if something other than caret visibility changed
    bool positionChanged = (state.cursorPosition != caretState.cursorPosition ||
                           state.selectionStart != caretState.selectionStart ||
                           state.selectionEnd != caretState.selectionEnd ||
                           state.hasSelection != caretState.hasSelection);
    caretState = state;
    if (positionChanged) {
        needsRepaint = true;
    }
}

void MarkdownRenderer::setFontFace(FT_Face face) {
    layoutEngine->setFontFace(face);
    needsRelayout = true;
    needsRepaint = true;
}

void MarkdownRenderer::setMonoFontFace(FT_Face face) {
    layoutEngine->setMonoFontFace(face);
    needsRelayout = true;
    needsRepaint = true;
}

void MarkdownRenderer::render(const Size& viewportSize, float scrollOffsetY) {
    if (needsReparse) {
        parseMarkdown();
    }

    if (viewportSize.width != lastViewportSize.width ||
        viewportSize.height != lastViewportSize.height) {
        needsRelayout = true;
        lastViewportSize = viewportSize;
    }

    if (needsRelayout) {
        performLayout(viewportSize);
    }

    if (needsRepaint) {
        paint();
    }

    rasterize(viewportSize, scrollOffsetY);
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

    const char* text = textBuffer ? textBuffer->getText().c_str() : nullptr;
    int textLen = textBuffer ? static_cast<int>(textBuffer->getText().length()) : 0;

    displayList = painter->paint(layoutTree.get(), &caretState, text, textLen);
    needsRepaint = false;
}

void MarkdownRenderer::rasterize(const Size& viewportSize, float scrollOffsetY) {
    Rect viewport(0, 0, viewportSize.width, viewportSize.height);
    rasterizer->rasterize(displayList, viewport, scrollOffsetY, caretState.caretVisible);
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

float MarkdownRenderer::getContentHeight() const {
    if (layoutTree) {
        return layoutTree->getRect().size.height;
    }
    return 0;
}

// Helper to collect text objects from object tree
static void collectTextObjects(const MarkdownObject* obj, std::vector<const MarkdownObject*>& out) {
    if (!obj) return;
    if (obj->getType() == MarkdownObjectType::Text) {
        out.push_back(obj);
    }
    for (const auto& child : obj->getChildren()) {
        collectTextObjects(child.get(), out);
    }
}

int MarkdownRenderer::getTotalDOMLength() const {
    if (!objectTree) return 0;

    std::vector<const MarkdownObject*> textObjects;
    collectTextObjects(objectTree.get(), textObjects);

    int total = 0;
    for (const auto* obj : textObjects) {
        int len = static_cast<int>(obj->getText().length());
        // Empty lines occupy 1 DOM position
        total += (len == 0) ? 1 : len;
    }
    return total;
}

int MarkdownRenderer::domToRaw(int domPos) const {
    if (!objectTree) return domPos;

    std::vector<const MarkdownObject*> textObjects;
    collectTextObjects(objectTree.get(), textObjects);

    int currentDOMPos = 0;
    for (const auto* obj : textObjects) {
        int textLen = static_cast<int>(obj->getText().length());
        int objLen = (textLen == 0) ? 1 : textLen;  // Empty lines occupy 1 DOM position

        if (domPos < currentDOMPos + objLen) {
            // Found the object containing this DOM position
            int localOffset = domPos - currentDOMPos;
            if (textLen == 0) {
                // Empty line - return the raw start
                return obj->getRawStart();
            }
            return obj->getRawStart() + localOffset;
        }
        currentDOMPos += objLen;
    }

    // Position is at or beyond end
    if (!textObjects.empty()) {
        return textObjects.back()->getRawEnd();
    }
    return domPos;
}

// Helper to collect TextLayoutObjects with their DOM position
static void collectTextLayoutsWithPos(const LayoutObject* obj,
                                       std::vector<std::pair<const TextLayoutObject*, int>>& out,
                                       int& currentDOMPos) {
    if (!obj) return;

    if (const auto* textLayout = dynamic_cast<const TextLayoutObject*>(obj)) {
        out.push_back({textLayout, currentDOMPos});
        int len = textLayout->getDOMLength();
        currentDOMPos += len;
    }

    for (const auto& child : obj->getChildren()) {
        collectTextLayoutsWithPos(child.get(), out, currentDOMPos);
    }
}

int MarkdownRenderer::hitTest(float x, float y) const {
    if (!layoutTree) return 0;

    std::vector<std::pair<const TextLayoutObject*, int>> textLayouts;
    int domPos = 0;
    collectTextLayoutsWithPos(layoutTree.get(), textLayouts, domPos);

    // Find which text layout and which LINE within it contains the click point
    // Must check BOTH x and y because inline elements share the same y range
    const TextLayoutObject* hitLayout = nullptr;
    int hitDOMStart = 0;
    int hitLineIdx = -1;

    for (const auto& [layout, layoutDOMPos] : textLayouts) {
        const Rect& rect = layout->getRect();
        const auto& lines = layout->getLines();
        float fontSize = layout->getFontSize();

        // Check each line within this layout
        for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
            const auto& line = lines[lineIdx];
            float lineY = rect.position.y + line.yOffset;
            float lineHeight = fontSize;

            // Check Y range
            if (y >= lineY && y < lineY + lineHeight) {
                // Also check X range for this line
                float lineStartX = rect.position.x;
                float lineEndX = rect.position.x + line.width;

                if (x >= lineStartX && x <= lineEndX) {
                    // Perfect match - x and y both within this text
                    hitLayout = layout;
                    hitDOMStart = layoutDOMPos;
                    hitLineIdx = static_cast<int>(lineIdx);
                    break;
                }
            }
        }
        if (hitLayout) break;
    }

    // If no exact match, find the text layout on the same line that's closest in X
    if (!hitLayout && !textLayouts.empty()) {
        // First, find all layouts at the clicked Y position
        std::vector<std::tuple<const TextLayoutObject*, int, int, float, float>> candidates;

        for (const auto& [layout, layoutDOMPos] : textLayouts) {
            const Rect& rect = layout->getRect();
            const auto& lines = layout->getLines();
            float fontSize = layout->getFontSize();

            for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
                const auto& line = lines[lineIdx];
                float lineY = rect.position.y + line.yOffset;
                float lineHeight = fontSize;

                if (y >= lineY && y < lineY + lineHeight) {
                    float lineStartX = rect.position.x;
                    float lineEndX = rect.position.x + line.width;
                    candidates.push_back({layout, layoutDOMPos, static_cast<int>(lineIdx), lineStartX, lineEndX});
                }
            }
        }

        if (!candidates.empty()) {
            // Find which candidate the x position falls into or is closest to
            for (const auto& [layout, layoutDOMPos, lineIdx, startX, endX] : candidates) {
                if (x < startX) {
                    // Click is before this element - if it's the first, use it
                    if (!hitLayout) {
                        hitLayout = layout;
                        hitDOMStart = layoutDOMPos;
                        hitLineIdx = lineIdx;
                    }
                } else if (x <= endX) {
                    // Click is within this element
                    hitLayout = layout;
                    hitDOMStart = layoutDOMPos;
                    hitLineIdx = lineIdx;
                    break;
                } else {
                    // Click is after this element - remember it as potential match
                    hitLayout = layout;
                    hitDOMStart = layoutDOMPos;
                    hitLineIdx = lineIdx;
                }
            }
        }
    }

    // If still no layout found, find closest by Y
    if (!hitLayout && !textLayouts.empty()) {
        float minDist = 1e9f;
        for (const auto& [layout, layoutDOMPos] : textLayouts) {
            const Rect& rect = layout->getRect();
            const auto& lines = layout->getLines();
            float fontSize = layout->getFontSize();

            for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
                const auto& line = lines[lineIdx];
                float lineY = rect.position.y + line.yOffset;
                float lineHeight = fontSize;
                float centerY = lineY + lineHeight / 2;
                float dist = std::abs(y - centerY);
                if (dist < minDist) {
                    minDist = dist;
                    hitLayout = layout;
                    hitDOMStart = layoutDOMPos;
                    hitLineIdx = static_cast<int>(lineIdx);
                }
            }
        }
    }

    if (!hitLayout) return 0;

    // Find character position within the specific line based on x
    const Rect& rect = hitLayout->getRect();
    const auto& lines = hitLayout->getLines();
    int charCount = hitLayout->getCharCount();

    if (charCount == 0 || lines.empty()) {
        // Empty line
        return domToRaw(hitDOMStart);
    }

    // Clamp hitLineIdx to valid range
    if (hitLineIdx < 0) hitLineIdx = 0;
    if (hitLineIdx >= static_cast<int>(lines.size())) hitLineIdx = static_cast<int>(lines.size()) - 1;

    const auto& line = lines[hitLineIdx];
    int lineStartChar = line.startChar;
    int lineEndChar = line.endChar;

    // If click is before the text start
    if (x <= rect.position.x) {
        return domToRaw(hitDOMStart + lineStartChar);
    }

    // Find which character on this line was clicked using line-relative x offset
    float relX = x - rect.position.x;

    for (int i = lineStartChar; i < lineEndChar; i++) {
        float charEndX = hitLayout->getCharXOffsetInLine(i + 1);
        float charStartX = (i == lineStartChar) ? 0 : hitLayout->getCharXOffsetInLine(i);
        float charMidX = (charStartX + charEndX) / 2;

        if (relX < charMidX) {
            // Click is in first half of char - position before it
            return domToRaw(hitDOMStart + i);
        }
    }

    // Click is after all characters on this line
    return domToRaw(hitDOMStart + lineEndChar);
}

float MarkdownRenderer::getCursorY(int domPos) const {
    if (!layoutTree) return 0;

    std::vector<std::pair<const TextLayoutObject*, int>> textLayouts;
    int pos = 0;
    collectTextLayoutsWithPos(layoutTree.get(), textLayouts, pos);

    // Find which text layout contains this DOM position
    for (const auto& [layout, layoutDOMPos] : textLayouts) {
        int domLen = layout->getDOMLength();
        if (domPos >= layoutDOMPos && domPos <= layoutDOMPos + domLen) {
            // Found the layout containing this position
            const Rect& rect = layout->getRect();
            float fontSize = layout->getFontSize();
            float lineHeight = fontSize;
            return rect.position.y + lineHeight;  // Return bottom of line
        }
    }

    // If not found, return bottom of last layout
    if (!textLayouts.empty()) {
        const auto& [layout, layoutDOMPos] = textLayouts.back();
        const Rect& rect = layout->getRect();
        float fontSize = layout->getFontSize();
        return rect.position.y + fontSize;
    }

    return 0;
}

void MarkdownRenderer::getCursorXY(int domPos, float& outX, float& outY) const {
    outX = 0;
    outY = 0;
    if (!layoutTree) return;

    std::vector<std::pair<const TextLayoutObject*, int>> textLayouts;
    int pos = 0;
    collectTextLayoutsWithPos(layoutTree.get(), textLayouts, pos);

    for (const auto& [layout, layoutDOMPos] : textLayouts) {
        int domLen = layout->getDOMLength();
        if (domPos >= layoutDOMPos && domPos <= layoutDOMPos + domLen) {
            const Rect& rect = layout->getRect();
            int localOffset = domPos - layoutDOMPos;

            const auto& lines = layout->getLines();
            if (!lines.empty() && localOffset >= 0) {
                int lineIdx = layout->getLineForChar(localOffset);
                const auto& line = lines[lineIdx];
                outY = rect.position.y + line.yOffset;
                outX = rect.position.x;
                if (localOffset > line.startChar) {
                    outX += layout->getCharXOffsetInLine(localOffset);
                }
            } else {
                outY = rect.position.y;
                outX = rect.position.x;
                if (localOffset > 0 && localOffset <= layout->getCharCount()) {
                    outX += layout->getCharXOffset(localOffset - 1);
                }
            }
            return;
        }
    }

    // Position beyond end
    if (!textLayouts.empty()) {
        const auto& [layout, layoutDOMPos] = textLayouts.back();
        (void)layoutDOMPos;
        const Rect& rect = layout->getRect();
        const auto& lines = layout->getLines();
        if (!lines.empty()) {
            outY = rect.position.y + lines.back().yOffset;
            outX = rect.position.x + lines.back().width;
        } else {
            outY = rect.position.y;
            outX = rect.position.x;
        }
    }
}

std::string MarkdownRenderer::getLinkAtPosition(float x, float y) const {
    if (!layoutTree) return "";

    std::vector<std::pair<const TextLayoutObject*, int>> textLayouts;
    int domPos = 0;
    collectTextLayoutsWithPos(layoutTree.get(), textLayouts, domPos);

    // Find which text layout contains the click point
    for (const auto& [layout, layoutDOMPos] : textLayouts) {
        const Rect& rect = layout->getRect();
        float fontSize = layout->getFontSize();
        const auto& lines = layout->getLines();
        const auto& linkRanges = layout->getLinkRanges();

        if (linkRanges.empty()) continue;

        // Check each line
        for (const auto& line : lines) {
            float lineY = rect.position.y + line.yOffset;
            float lineHeight = fontSize;

            if (y >= lineY && y < lineY + lineHeight) {
                // Click is on this line - find character position
                float relX = x - rect.position.x;
                int charPos = line.startChar;

                // Find which character was clicked
                for (int i = line.startChar; i < line.endChar; i++) {
                    float charEndX = layout->getCharXOffsetInLine(i + 1);
                    if (relX < charEndX) {
                        charPos = i;
                        break;
                    }
                    charPos = i;
                }

                // Check if this character is in a link range
                for (const auto& lr : linkRanges) {
                    if (charPos >= lr.startChar && charPos < lr.endChar) {
                        return lr.url;
                    }
                }
            }
        }
    }

    return "";
}

int MarkdownRenderer::rawToDOM(int rawPos) const {
    if (!objectTree) return rawPos;

    std::vector<const MarkdownObject*> textObjects;
    collectTextObjects(objectTree.get(), textObjects);

    // If position is before first text object, return 0
    if (!textObjects.empty() && rawPos < textObjects[0]->getRawStart()) {
        return 0;
    }

    int currentDOMPos = 0;
    for (size_t i = 0; i < textObjects.size(); i++) {
        const auto* obj = textObjects[i];
        int rawStart = obj->getRawStart();
        int rawEnd = obj->getRawEnd();
        int textLen = static_cast<int>(obj->getText().length());
        int objDOMLen = (textLen == 0) ? 1 : textLen;  // Empty lines occupy 1 DOM position

        if (rawPos >= rawStart && rawPos <= rawEnd) {
            // Raw position is within this object's text range
            int localOffset = rawPos - rawStart;
            if (textLen == 0) {
                // Empty line - just return current DOM position
                return currentDOMPos;
            }
            localOffset = std::min(localOffset, textLen);
            return currentDOMPos + localOffset;
        }

        currentDOMPos += objDOMLen;

        // Check if position is between this object and the next
        if (i + 1 < textObjects.size()) {
            int nextRawStart = textObjects[i + 1]->getRawStart();
            if (rawPos > rawEnd && rawPos < nextRawStart) {
                // Position is in the gap between objects (e.g., in \n\n)
                // Return end of current object
                return currentDOMPos;
            }
        }
    }

    // Position is beyond all text objects
    return currentDOMPos;
}