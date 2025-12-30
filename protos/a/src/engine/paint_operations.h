#pragma once
#include "layout_objects.h"
#include <vector>
#include <memory>
#include <cstdint>

struct Color {
    uint8_t r, g, b, a;
    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255) 
        : r(r), g(g), b(b), a(a) {}
};

enum class PaintOpType {
    DrawRect,
    DrawText,
    DrawImage,
    SetClip,
    RestoreClip,
    DrawDebugBorder,
    DrawCaret,
    DrawSelectionRect
};

class PaintOp {
public:
    PaintOp(PaintOpType type);
    virtual ~PaintOp();
    
    PaintOpType getType() const;
    
private:
    PaintOpType type;
};

class DrawRectOp : public PaintOp {
public:
    DrawRectOp(const Rect& rect, const Color& color);
    
    const Rect& getRect() const;
    const Color& getColor() const;
    
private:
    Rect rect;
    Color color;
};

class DrawTextOp : public PaintOp {
public:
    DrawTextOp(const Point& position, const std::string& text, const Color& color, float fontSize = 16.0f);
    
    const Point& getPosition() const;
    const std::string& getText() const;
    const Color& getColor() const;
    float getFontSize() const;
    
private:
    Point position;
    std::string text;
    Color color;
    float fontSize;
};

class DrawImageOp : public PaintOp {
public:
    DrawImageOp(const Rect& destRect, const std::string& imagePath);
    
    const Rect& getDestRect() const;
    const std::string& getImagePath() const;
    
private:
    Rect destRect;
    std::string imagePath;
};

class SetClipOp : public PaintOp {
public:
    SetClipOp(const Rect& clipRect);
    
    const Rect& getClipRect() const;
    
private:
    Rect clipRect;
};

class RestoreClipOp : public PaintOp {
public:
    RestoreClipOp();
};

class DrawDebugBorderOp : public PaintOp {
public:
    DrawDebugBorderOp(const Rect& rect, const Color& color);

    const Rect& getRect() const;
    const Color& getColor() const;

private:
    Rect rect;
    Color color;
};

// Caret (text cursor) - thin vertical line
class DrawCaretOp : public PaintOp {
public:
    DrawCaretOp(const Point& position, float height, const Color& color);

    const Point& getPosition() const;
    float getHeight() const;
    const Color& getColor() const;

private:
    Point position;
    float height;
    Color color;
};

// Selection highlight rectangle (painted behind text)
class DrawSelectionRectOp : public PaintOp {
public:
    DrawSelectionRectOp(const Rect& rect, const Color& color);

    const Rect& getRect() const;
    const Color& getColor() const;

private:
    Rect rect;
    Color color;
};

using DisplayList = std::vector<std::unique_ptr<PaintOp>>;