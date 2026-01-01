#include "markdown_renderer.h"
#include <chrono>
#include <iostream>

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
    caretState = state;
    needsRepaint = true;  // Caret changes require repaint
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

void MarkdownRenderer::render(const Size& viewportSize) {
    using Clock = std::chrono::high_resolution_clock;
    auto frameStart = Clock::now();
    bool didWork = needsReparse || needsRelayout || needsRepaint;

    if (needsReparse) {
        auto t0 = Clock::now();
        parseMarkdown();
        auto t1 = Clock::now();
        std::cout << "[PROFILE] parse: "
                  << std::chrono::duration<double, std::milli>(t1 - t0).count() << "ms\n";
    }

    // Check if viewport size changed - need to relayout
    if (viewportSize.width != lastViewportSize.width ||
        viewportSize.height != lastViewportSize.height) {
        needsRelayout = true;
        lastViewportSize = viewportSize;
    }

    if (needsRelayout) {
        auto t0 = Clock::now();
        performLayout(viewportSize);
        auto t1 = Clock::now();
        std::cout << "[PROFILE] layout: "
                  << std::chrono::duration<double, std::milli>(t1 - t0).count() << "ms\n";
    }

    if (needsRepaint) {
        auto t0 = Clock::now();
        paint();
        auto t1 = Clock::now();
        std::cout << "[PROFILE] paint: "
                  << std::chrono::duration<double, std::milli>(t1 - t0).count() << "ms\n";
    }

    {
        auto t0 = Clock::now();
        rasterize(viewportSize);
        auto t1 = Clock::now();
        if (didWork) {
            std::cout << "[PROFILE] rasterize: "
                      << std::chrono::duration<double, std::milli>(t1 - t0).count() << "ms\n";
            auto frameEnd = Clock::now();
            std::cout << "[PROFILE] TOTAL: "
                      << std::chrono::duration<double, std::milli>(frameEnd - frameStart).count() << "ms\n\n";
        }
    }
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

    // Find which text layout contains the click point
    const TextLayoutObject* hitLayout = nullptr;
    int hitDOMStart = 0;

    for (const auto& [layout, layoutDOMPos] : textLayouts) {
        const Rect& rect = layout->getRect();
        float layoutHeight = rect.size.height;
        if (layoutHeight <= 0) {
            layoutHeight = layout->getFontSize();  // Fallback
        }

        // Check if y is within this layout's vertical bounds
        if (y >= rect.position.y && y < rect.position.y + layoutHeight) {
            hitLayout = layout;
            hitDOMStart = layoutDOMPos;
            break;
        }
    }

    // If no layout found at y, find closest one
    if (!hitLayout && !textLayouts.empty()) {
        float minDist = 1e9f;
        for (const auto& [layout, layoutDOMPos] : textLayouts) {
            const Rect& rect = layout->getRect();
            float layoutHeight = rect.size.height;
            if (layoutHeight <= 0) {
                layoutHeight = layout->getFontSize();
            }
            float centerY = rect.position.y + layoutHeight / 2;
            float dist = std::abs(y - centerY);
            if (dist < minDist) {
                minDist = dist;
                hitLayout = layout;
                hitDOMStart = layoutDOMPos;
            }
        }
    }

    if (!hitLayout) return 0;

    // Find character position within the layout based on x
    const Rect& rect = hitLayout->getRect();
    int charCount = hitLayout->getCharCount();

    if (charCount == 0) {
        // Empty line
        return domToRaw(hitDOMStart);
    }

    // If click is before the text start
    if (x <= rect.position.x) {
        return domToRaw(hitDOMStart);
    }

    // Find which character was clicked
    float relX = x - rect.position.x;
    for (int i = 0; i < charCount; i++) {
        float charEnd = hitLayout->getCharXOffset(i);
        float charStart = (i == 0) ? 0 : hitLayout->getCharXOffset(i - 1);
        float charMid = (charStart + charEnd) / 2;

        if (relX < charMid) {
            // Click is in first half of char - position before it
            return domToRaw(hitDOMStart + i);
        }
    }

    // Click is after all characters
    return domToRaw(hitDOMStart + charCount);
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