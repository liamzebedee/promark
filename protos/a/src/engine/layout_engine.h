#pragma once
#include "markdown_objects.h"
#include "layout_objects.h"
#include <memory>

class LayoutEngine {
public:
    LayoutEngine();
    ~LayoutEngine();
    
    std::unique_ptr<LayoutObject> createLayoutTree(const MarkdownObject* objectTree);
    void performLayout(LayoutObject* layoutRoot, const Size& availableSpace);
    
private:
    std::unique_ptr<LayoutObject> createLayoutObject(const MarkdownObject* object);
    void layoutBlockFlow(LayoutObject* layoutObject, const Size& availableSpace);
    void layoutInlineFlow(LayoutObject* layoutObject, const Size& availableSpace);
};