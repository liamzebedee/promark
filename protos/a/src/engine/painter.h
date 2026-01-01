#pragma once
#include "paint_operations.h"
#include "layout_objects.h"
#include <cstdlib>
#include <vector>

// Forward declaration - defined in markdown_renderer.h
struct CaretState;

// Result of DOM position lookup
struct DOMPositionResult {
    const LayoutObject* layout = nullptr;  // Which layout object
    int localOffset = 0;                   // Offset within that object
    bool isAtomicBoundary = false;         // True if cursor is at edge of atomic element
};

class Painter {
public:
    Painter();
    ~Painter();

    DisplayList paint(const LayoutObject* layoutRoot, const CaretState* caret = nullptr,
                      const char* text = nullptr, int textLength = 0);

private:
    void paintLayoutObject(const LayoutObject* layoutObject, DisplayList& displayList);
    void paintText(const TextLayoutObject* textObject, DisplayList& displayList);
    void paintImage(const ImageLayoutObject* imageObject, DisplayList& displayList);
    void paintBackground(const LayoutObject* layoutObject, DisplayList& displayList);
    void paintBorder(const LayoutObject* layoutObject, DisplayList& displayList);
    void paintDebugBorder(const LayoutObject* layoutObject, DisplayList& displayList);
    void paintBlockQuoteBar(const LayoutObject* layoutObject, DisplayList& displayList);
    void paintLinkUnderline(const TextLayoutObject* textObject, DisplayList& displayList);
    bool isInsideLink(const LayoutObject* layoutObject);

    // Caret/selection painting
    void paintCaret(DisplayList& displayList, const CaretState& caret,
                    const char* text, int textLength, const LayoutObject* layoutRoot);
    void paintSelection(DisplayList& displayList, const CaretState& caret,
                        const char* text, int textLength, const LayoutObject* layoutRoot);

    // Helper to compute x position for a character offset within text
    float computeXForOffset(int offset, const char* text, float fontSize, float startX);

    // DOM position helpers
    DOMPositionResult findLayoutForPosition(const LayoutObject* root, int domPosition);
    void collectContentLayouts(const LayoutObject* obj, std::vector<const LayoutObject*>& out);
    Rect computeSelectionRect(const LayoutObject* layout, int localStart, int localEnd);

    Color getTextColor(const MarkdownObject* object);
    Color getBackgroundColor(const MarkdownObject* object);
};