#pragma once
#include "layout_objects.h"
#include "markdown_objects.h"  // For TextStyle
#include <vector>
#include <memory>
#include <cstdint>

struct Color {
    uint8_t r, g, b, a;
    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}
};

// RectRole identifies the semantic purpose of a DrawRect operation.
// This enables the rasterizer to handle different rect types uniformly
// while preserving their rendering behavior (e.g., border vs filled).
enum class RectRole {
    Background,  // Solid background fill (e.g., table header, code block bg)
    Selection,   // Text selection highlight (painted behind text)
    Caret,       // Text cursor (thin vertical line)
    Border,      // Element border (stroke, not fill)
    Debug        // Debug visualization (layout bounds)
};

enum class PaintOpType {
    DrawRect,
    DrawText,
    DrawImage,
    DrawLine
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
    // Full constructor with role (for explicit rect semantics)
    DrawRectOp(const Rect& rect, const Color& color, RectRole role);

    // Convenience constructor defaulting to Background role (backwards compatible)
    DrawRectOp(const Rect& rect, const Color& color);

    const Rect& getRect() const;
    const Color& getColor() const;
    RectRole getRole() const;

private:
    Rect rect;
    Color color;
    RectRole role;
};

class DrawTextOp : public PaintOp {
public:
    DrawTextOp(const Point& position, const std::string& text, const Color& color,
               float fontSize = 16.0f, TextStyle style = TextStyle::Normal,
               bool monospace = false);

    const Point& getPosition() const;
    const std::string& getText() const;
    const Color& getColor() const;
    float getFontSize() const;
    TextStyle getStyle() const;
    bool isMonospace() const;

private:
    Point position;
    std::string text;
    Color color;
    float fontSize;
    TextStyle style;
    bool monospace;
};

class DrawImageOp : public PaintOp {
public:
    // Full constructor with all fields
    DrawImageOp(const Rect& destRect, const std::string& imagePath,
                uint32_t textureId, const Rect& sourceRect, const Color& tintColor);

    // Convenience constructor for path-based loading (textureId=0, full source rect, no tint)
    DrawImageOp(const Rect& destRect, const std::string& imagePath);

    const Rect& getDestRect() const;
    const std::string& getImagePath() const;
    uint32_t getTextureId() const;
    const Rect& getSourceRect() const;
    const Color& getTintColor() const;

private:
    Rect destRect;
    std::string imagePath;
    uint32_t textureId;      // Pre-loaded texture ID (0 = load from imagePath)
    Rect sourceRect;         // Source rect for atlasing (normalized 0-1 coords)
    Color tintColor;         // Tint/multiply color (white = no tint)
};

// Line (for underlines, blockquote bars)
class DrawLineOp : public PaintOp {
public:
    DrawLineOp(const Point& start, const Point& end, float thickness, const Color& color);

    const Point& getStart() const;
    const Point& getEnd() const;
    float getThickness() const;
    const Color& getColor() const;

private:
    Point start;
    Point end;
    float thickness;
    Color color;
};

using DisplayList = std::vector<std::unique_ptr<PaintOp>>;

// PaintArtifact represents a node in the paint tree hierarchy.
// It mirrors the layout tree structure to enable efficient culling and caching.
//
// Benefits of tree structure:
// - Culling: Rasterizer can skip entire subtrees outside viewport
// - Clipping: Clip regions are structural properties, not stateful push/pop
// - Caching: Unchanged subtrees can be reused
// - Debugging: Paint tree mirrors layout tree structure
//
// Per spec: specs/03-rendering-pipeline.md
class PaintArtifact {
public:
    PaintArtifact();
    explicit PaintArtifact(const Rect& bounds);

    // Display items for this node (drawing commands)
    DisplayList displayItems;

    // Bounding box for culling - if bounds don't intersect viewport, skip entire subtree
    Rect bounds;

    // Optional clip region (structural, not push/pop commands)
    bool hasClip;
    Rect clipRect;

    // Child artifacts (owned by this artifact)
    std::vector<std::unique_ptr<PaintArtifact>> children;

    // Add a child artifact
    void addChild(std::unique_ptr<PaintArtifact> child);

    // Add a display item to this artifact
    void addDisplayItem(std::unique_ptr<PaintOp> item);

    // Set clip region
    void setClip(const Rect& clip);
    void clearClip();
};

// PaintTree is the root of the hierarchical paint artifact tree.
// The Painter produces this, and the Rasterizer consumes it.
using PaintTree = std::unique_ptr<PaintArtifact>;