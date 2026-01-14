#pragma once
#include "markdown_objects.h"
#include "font_provider.h"
#include <vector>
#include <memory>

struct Point {
    float x, y;
    Point(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Size {
    float width, height;
    Size(float w = 0, float h = 0) : width(w), height(h) {}
};

struct Rect {
    Point position;
    Size size;
    Rect(float x = 0, float y = 0, float w = 0, float h = 0)
        : position(x, y), size(w, h) {}
};

// Box model edge insets for margin and padding (P2-5)
struct EdgeInsets {
    float top, right, bottom, left;
    EdgeInsets(float t = 0, float r = 0, float b = 0, float l = 0)
        : top(t), right(r), bottom(b), left(l) {}
    // Convenience constructors
    static EdgeInsets all(float value) { return EdgeInsets(value, value, value, value); }
    static EdgeInsets symmetric(float vertical, float horizontal) {
        return EdgeInsets(vertical, horizontal, vertical, horizontal);
    }
    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
};

enum class LayoutFlow {
    Block
};

class LayoutObject {
public:
    LayoutObject(const MarkdownObject* sourceObject, LayoutFlow flow);
    virtual ~LayoutObject();

    const MarkdownObject* getSourceObject() const;
    LayoutFlow getFlow() const;

    void setRect(const Rect& rect);
    const Rect& getRect() const;

    // Box model (P2-5): margin and padding
    void setMargin(const EdgeInsets& m) { margin = m; }
    void setPadding(const EdgeInsets& p) { padding = p; }
    const EdgeInsets& getMargin() const { return margin; }
    const EdgeInsets& getPadding() const { return padding; }

    // Get content box (alias for getRect for clarity)
    const Rect& getContentBox() const { return rect; }

    // Get padding box (content box expanded by padding)
    Rect getPaddingBox() const;

    // Get margin box (padding box expanded by margin)
    Rect getMarginBox() const;

    void addChild(std::unique_ptr<LayoutObject> child);
    const std::vector<std::unique_ptr<LayoutObject>>& getChildren() const;

    virtual Size computeIntrinsicSize() const;
    virtual void layout(const Size& availableSpace);
    virtual float getFontSize() const;

    // DOM position support - for cursor/selection
    virtual int getDOMLength() const { return 0; }  // Container = 0
    virtual bool isAtomic() const { return false; }

    void setParent(LayoutObject* parent);
    LayoutObject* getParent() const;

    // Invalidate cached styles when hierarchy changes
    void invalidateFontSizeCache();

protected:
    const MarkdownObject* sourceObject;
    LayoutFlow flow;
    Rect rect;
    std::vector<std::unique_ptr<LayoutObject>> children;
    LayoutObject* parent;

    // Box model (P2-5): margin and padding values
    EdgeInsets margin;   // Outer spacing from other elements
    EdgeInsets padding;  // Inner spacing around content

    // Cached font size (P1-5: Pre-computed styles)
    mutable float cachedFontSize = -1.0f;  // -1 = not computed
    mutable LayoutObject* cachedFontSizeParent = nullptr;  // For invalidation check
};

class BlockLayoutObject : public LayoutObject {
public:
    BlockLayoutObject(const MarkdownObject* sourceObject);
    void layout(const Size& availableSpace) override;
};


// Pre-computed character-level styles (P1-5: Fully resolved style information)
// Computed once during layout, reused by painter without recomputation
struct ComputedCharStyles {
    std::vector<TextStyle> charStyles;    // TextStyle for each character
    std::vector<int> charLinkIdx;         // Link index (-1 = no link) for each character
    std::vector<bool> isCodeStyle;        // Fast lookup for code styling

    // Validation metadata
    int cachedCharCount = -1;
    size_t cachedStyleRangeCount = 0;
    size_t cachedLinkRangeCount = 0;
};

class TextLayoutObject : public LayoutObject {
public:
    TextLayoutObject(const MarkdownObject* sourceObject);
    Size computeIntrinsicSize() const override;
    void layout(const Size& availableSpace) override;

    struct GlyphRun {
        std::vector<uint32_t> glyphIds;
        std::vector<Point> positions;
        float width;
        float height;
    };

    struct LineInfo {
        int startChar;
        int endChar;
        float yOffset;
        float width;
    };

    const std::vector<GlyphRun>& getGlyphRuns() const;
    const std::vector<LineInfo>& getLines() const;
    float getFontSize() const override;

    // DOM position support
    int getDOMLength() const override;
    int getCharCount() const;
    float getCharXOffset(int index) const;  // Cumulative x offset after char at index
    int getLineForChar(int charIndex) const;
    float getCharXOffsetInLine(int charIndex) const;

    // Font provider for glyph metrics (does NOT take ownership)
    void setFontProvider(const FontProvider* provider);
    void setMonospace(bool mono) { isMonospace = mono; }
    bool getMonospace() const { return isMonospace; }

    // Link ranges from parent paragraph
    const std::vector<InlineLinkRange>& getLinkRanges() const;

    // Style ranges from parent paragraph
    const std::vector<InlineStyleRange>& getStyleRanges() const;

    // P1-5: Pre-computed character styles for painter (computed during layout)
    const ComputedCharStyles& getComputedCharStyles() const { return computedCharStyles; }

private:
    std::vector<GlyphRun> glyphRuns;
    std::vector<float> charXOffsets;  // Cumulative x offset for each character
    std::vector<LineInfo> lines;
    const FontProvider* fontProvider;
    float availableWidth;
    bool isMonospace = false;

    // P1-5: Pre-computed styles (resolved once during layout)
    ComputedCharStyles computedCharStyles;

    void shapeText();
    void wrapText(float maxWidth);
    void computeCharStyles();  // P1-5: Compute character-level styles
};

class ImageLayoutObject : public LayoutObject {
public:
    ImageLayoutObject(const MarkdownObject* sourceObject);
    Size computeIntrinsicSize() const override;
    void layout(const Size& availableSpace) override;

    // DOM position support - images are atomic (1 position)
    int getDOMLength() const override { return 1; }
    bool isAtomic() const override { return true; }

private:
    mutable Size intrinsicSize;
    mutable bool sizeComputed;
    void computeImageSize() const;
};

class TableLayoutObject : public LayoutObject {
public:
    TableLayoutObject(const MarkdownObject* sourceObject);
    void layout(const Size& availableSpace) override;

    const std::vector<float>& getColumnWidths() const { return columnWidths; }

private:
    std::vector<float> columnWidths;
    void computeColumnWidths(float availableWidth);
};

class TableRowLayoutObject : public LayoutObject {
public:
    TableRowLayoutObject(const MarkdownObject* sourceObject);
    void layout(const Size& availableSpace) override;

    bool isHeader() const;
};

class TableCellLayoutObject : public LayoutObject {
public:
    TableCellLayoutObject(const MarkdownObject* sourceObject);
    void layout(const Size& availableSpace) override;

    TableCellAlign getAlignment() const;
};

class ListItemLayoutObject : public LayoutObject {
public:
    ListItemLayoutObject(const MarkdownObject* sourceObject);
    void layout(const Size& availableSpace) override;

    ListMarkerType getMarkerType() const;
    const std::string& getMarkerText() const;
    int getIndentLevel() const;
};