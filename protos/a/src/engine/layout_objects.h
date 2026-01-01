#pragma once
#include "markdown_objects.h"
#include <ft2build.h>
#include FT_FREETYPE_H
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

enum class LayoutFlow {
    Block,
    Inline
};

class LayoutObject {
public:
    LayoutObject(const MarkdownObject* sourceObject, LayoutFlow flow);
    virtual ~LayoutObject();
    
    const MarkdownObject* getSourceObject() const;
    LayoutFlow getFlow() const;
    
    void setRect(const Rect& rect);
    const Rect& getRect() const;
    
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
    
protected:
    const MarkdownObject* sourceObject;
    LayoutFlow flow;
    Rect rect;
    std::vector<std::unique_ptr<LayoutObject>> children;
    LayoutObject* parent;
};

class BlockLayoutObject : public LayoutObject {
public:
    BlockLayoutObject(const MarkdownObject* sourceObject);
    void layout(const Size& availableSpace) override;
};

class InlineLayoutObject : public LayoutObject {
public:
    InlineLayoutObject(const MarkdownObject* sourceObject);
    void layout(const Size& availableSpace) override;
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

    // FreeType face for glyph metrics
    void setFontFace(FT_Face face);

private:
    std::vector<GlyphRun> glyphRuns;
    std::vector<float> charXOffsets;  // Cumulative x offset for each character
    std::vector<LineInfo> lines;
    FT_Face fontFace;
    float availableWidth;
    void shapeText();
    void wrapText(float maxWidth);
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