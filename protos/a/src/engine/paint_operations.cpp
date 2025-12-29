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

DrawTextOp::DrawTextOp(const Point& position, const std::string& text, const Color& color) 
    : PaintOp(PaintOpType::DrawText), position(position), text(text), color(color) {
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

DrawImageOp::DrawImageOp(const Rect& destRect, const std::string& imagePath) 
    : PaintOp(PaintOpType::DrawImage), destRect(destRect), imagePath(imagePath) {
}

const Rect& DrawImageOp::getDestRect() const {
    return destRect;
}

const std::string& DrawImageOp::getImagePath() const {
    return imagePath;
}

SetClipOp::SetClipOp(const Rect& clipRect) 
    : PaintOp(PaintOpType::SetClip), clipRect(clipRect) {
}

const Rect& SetClipOp::getClipRect() const {
    return clipRect;
}

RestoreClipOp::RestoreClipOp() : PaintOp(PaintOpType::RestoreClip) {
}