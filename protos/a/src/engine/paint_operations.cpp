#include "paint_operations.h"

PaintOp::PaintOp(PaintOpType type) : type(type) {
}

PaintOp::~PaintOp() {
}

PaintOpType PaintOp::getType() const {
    return type;
}

DrawRectOp::DrawRectOp(const Rect& rect, const Color& color) 
    : PaintOp(PaintOpType::DrawRect), rect(rect), color(color) {
}

const Rect& DrawRectOp::getRect() const {
    return rect;
}

const Color& DrawRectOp::getColor() const {
    return color;
}

DrawTextOp::DrawTextOp(const Point& position, const std::string& text, const Color& color,
                       float fontSize, TextStyle style, bool monospace)
    : PaintOp(PaintOpType::DrawText), position(position), text(text), color(color),
      fontSize(fontSize), style(style), monospace(monospace) {
}

const Point& DrawTextOp::getPosition() const {
    return position;
}

const std::string& DrawTextOp::getText() const {
    return text;
}

const Color& DrawTextOp::getColor() const {
    return color;
}

float DrawTextOp::getFontSize() const {
    return fontSize;
}

TextStyle DrawTextOp::getStyle() const {
    return style;
}

bool DrawTextOp::isMonospace() const {
    return monospace;
}

// Full constructor with all fields
DrawImageOp::DrawImageOp(const Rect& destRect, const std::string& imagePath,
                         uint32_t textureId, const Rect& sourceRect, const Color& tintColor)
    : PaintOp(PaintOpType::DrawImage), destRect(destRect), imagePath(imagePath),
      textureId(textureId), sourceRect(sourceRect), tintColor(tintColor) {
}

// Convenience constructor: defaults to load-from-path, full source rect, white tint
DrawImageOp::DrawImageOp(const Rect& destRect, const std::string& imagePath)
    : PaintOp(PaintOpType::DrawImage), destRect(destRect), imagePath(imagePath),
      textureId(0), sourceRect(0, 0, 1, 1), tintColor(255, 255, 255, 255) {
}

const Rect& DrawImageOp::getDestRect() const {
    return destRect;
}

const std::string& DrawImageOp::getImagePath() const {
    return imagePath;
}

uint32_t DrawImageOp::getTextureId() const {
    return textureId;
}

const Rect& DrawImageOp::getSourceRect() const {
    return sourceRect;
}

const Color& DrawImageOp::getTintColor() const {
    return tintColor;
}

DrawDebugBorderOp::DrawDebugBorderOp(const Rect& rect, const Color& color) 
    : PaintOp(PaintOpType::DrawDebugBorder), rect(rect), color(color) {
}

const Rect& DrawDebugBorderOp::getRect() const {
    return rect;
}

const Color& DrawDebugBorderOp::getColor() const {
    return color;
}

// DrawCaretOp
DrawCaretOp::DrawCaretOp(const Point& position, float height, const Color& color)
    : PaintOp(PaintOpType::DrawCaret), position(position), height(height), color(color) {
}

const Point& DrawCaretOp::getPosition() const {
    return position;
}

float DrawCaretOp::getHeight() const {
    return height;
}

const Color& DrawCaretOp::getColor() const {
    return color;
}

// DrawSelectionRectOp
DrawSelectionRectOp::DrawSelectionRectOp(const Rect& rect, const Color& color)
    : PaintOp(PaintOpType::DrawSelectionRect), rect(rect), color(color) {
}

const Rect& DrawSelectionRectOp::getRect() const {
    return rect;
}

const Color& DrawSelectionRectOp::getColor() const {
    return color;
}

// DrawLineOp
DrawLineOp::DrawLineOp(const Point& start, const Point& end, float thickness, const Color& color)
    : PaintOp(PaintOpType::DrawLine), start(start), end(end), thickness(thickness), color(color) {
}

const Point& DrawLineOp::getStart() const {
    return start;
}

const Point& DrawLineOp::getEnd() const {
    return end;
}

float DrawLineOp::getThickness() const {
    return thickness;
}

const Color& DrawLineOp::getColor() const {
    return color;
}