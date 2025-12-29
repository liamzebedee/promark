#pragma once
#include "paint_operations.h"
#include "layout_objects.h"

class Painter {
public:
    Painter();
    ~Painter();
    
    DisplayList paint(const LayoutObject* layoutRoot);
    
private:
    void paintLayoutObject(const LayoutObject* layoutObject, DisplayList& displayList);
    void paintText(const TextLayoutObject* textObject, DisplayList& displayList);
    void paintImage(const ImageLayoutObject* imageObject, DisplayList& displayList);
    void paintBackground(const LayoutObject* layoutObject, DisplayList& displayList);
    void paintBorder(const LayoutObject* layoutObject, DisplayList& displayList);
    
    void resetTextPositioning();
    
    Color getTextColor(const MarkdownObject* object);
    Color getBackgroundColor(const MarkdownObject* object);
};