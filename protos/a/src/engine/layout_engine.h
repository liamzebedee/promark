#pragma once
#include "markdown_objects.h"
#include "layout_objects.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <memory>

class LayoutEngine {
public:
    LayoutEngine();
    ~LayoutEngine();

    void setFontFace(FT_Face face);
    void setMonoFontFace(FT_Face face);
    FT_Face getFontFace() const;
    FT_Face getMonoFontFace() const;

    std::unique_ptr<LayoutObject> createLayoutTree(const MarkdownObject* objectTree);
    void performLayout(LayoutObject* layoutRoot, const Size& availableSpace);

private:
    FT_Face fontFace;
    FT_Face monoFontFace;

    std::unique_ptr<LayoutObject> createLayoutObject(const MarkdownObject* object, bool inCodeBlock = false);
    std::unique_ptr<LayoutObject> createLayoutTree(const MarkdownObject* objectTree, bool inCodeBlock);
    void layoutBlockFlow(LayoutObject* layoutObject, const Size& availableSpace);
    void layoutInlineFlow(LayoutObject* layoutObject, const Size& availableSpace);
    void propagatePositionToChildren(LayoutObject* parent, float parentX, float parentY);
};