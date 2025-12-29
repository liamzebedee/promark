#pragma once
#include "markdown_objects.h"
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
    
    const std::vector<GlyphRun>& getGlyphRuns() const;
    float getFontSize() const;
    
private:
    std::vector<GlyphRun> glyphRuns;
    void shapeText();
};

class ImageLayoutObject : public LayoutObject {
public:
    ImageLayoutObject(const MarkdownObject* sourceObject);
    Size computeIntrinsicSize() const override;
    void layout(const Size& availableSpace) override;
    
private:
    Size intrinsicSize;
    bool sizeComputed;
    void computeImageSize();
};