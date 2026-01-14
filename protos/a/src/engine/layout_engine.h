#pragma once
#include "markdown_objects.h"
#include "layout_objects.h"
#include "font_provider.h"
#include <memory>

class LayoutEngine {
public:
    LayoutEngine();
    ~LayoutEngine();

    // Set font provider for text measurement
    // LayoutEngine does NOT take ownership - provider must outlive the engine
    void setFontProvider(const FontProvider* provider);
    const FontProvider* getFontProvider() const;

    std::unique_ptr<LayoutObject> createLayoutTree(const MarkdownObject* objectTree);
    void performLayout(LayoutObject* layoutRoot, const Size& availableSpace);

private:
    const FontProvider* fontProvider;

    std::unique_ptr<LayoutObject> createLayoutObject(const MarkdownObject* object, bool inCodeBlock = false);
    std::unique_ptr<LayoutObject> createLayoutTree(const MarkdownObject* objectTree, bool inCodeBlock);
    void layoutBlockFlow(LayoutObject* layoutObject, const Size& availableSpace);
    void layoutInlineFlow(LayoutObject* layoutObject, const Size& availableSpace);
    void propagatePositionToChildren(LayoutObject* parent, float parentX, float parentY);

    // Unified layout authority: LayoutEngine positions all objects
    void layoutTable(LayoutObject* table, const Size& availableSpace);
    void layoutTableRow(LayoutObject* row, const std::vector<float>& columnWidths, const Size& availableSpace);
    void layoutTableCell(LayoutObject* cell, TableCellAlign alignment, const Size& availableSpace);
    void layoutListItem(LayoutObject* listItem, const Size& availableSpace);
    std::vector<float> computeTableColumnWidths(LayoutObject* table, float availableWidth);
};