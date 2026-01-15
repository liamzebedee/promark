#include "layout_engine.h"
#include "typography.h"
#include <iostream>

LayoutEngine::LayoutEngine() : fontProvider(nullptr) {
}

LayoutEngine::~LayoutEngine() {
}

void LayoutEngine::setFontProvider(const FontProvider* provider) {
    fontProvider = provider;
}

const FontProvider* LayoutEngine::getFontProvider() const {
    return fontProvider;
}

std::unique_ptr<LayoutObject> LayoutEngine::createLayoutTree(const MarkdownObject* objectTree, bool inCodeBlock) {
    if (!objectTree) {
        return nullptr;
    }

    bool isCodeBlock = (objectTree->getType() == MarkdownObjectType::CodeBlock);
    bool isFrontmatter = (objectTree->getType() == MarkdownObjectType::Frontmatter);
    auto layoutObject = createLayoutObject(objectTree, inCodeBlock || isFrontmatter);

    // Create layout objects for children
    for (const auto& child : objectTree->getChildren()) {
        auto childLayout = createLayoutTree(child.get(), isCodeBlock || isFrontmatter || inCodeBlock);
        if (childLayout) {
            layoutObject->addChild(std::move(childLayout));
        }
    }

    return layoutObject;
}

std::unique_ptr<LayoutObject> LayoutEngine::createLayoutTree(const MarkdownObject* objectTree) {
    return createLayoutTree(objectTree, false);
}

void LayoutEngine::performLayout(LayoutObject* layoutRoot, const Size& availableSpace) {
    if (!layoutRoot) {
        return;
    }

    // Handle atomic layout objects (like images) that compute their own size
    if (layoutRoot->isAtomic()) {
        layoutRoot->layout(availableSpace);
        return;
    }

    // Unified layout authority: LayoutEngine handles all positioning
    // No special-case delegation - engine is the single coordinator
    const MarkdownObject* sourceObj = layoutRoot->getSourceObject();
    if (sourceObj) {
        MarkdownObjectType type = sourceObj->getType();
        if (type == MarkdownObjectType::Table) {
            layoutTable(layoutRoot, availableSpace);
            return;
        }
        // Text objects need their layout() called for shaping/wrapping
        if (type == MarkdownObjectType::Text) {
            layoutRoot->layout(availableSpace);
            return;
        }
        // ThematicBreak has fixed height (no children to compute from)
        if (type == MarkdownObjectType::ThematicBreak) {
            float hrHeight = Typography::BLOCK_SPACING;  // Use standard spacing as height
            layoutRoot->setRect(Rect(0, 0, availableSpace.width, hrHeight));
            return;
        }
        // TableRow and TableCell are handled by layoutTable/layoutTableRow
        // ListItem is handled by layoutBlockFlow
    }

    // All layout objects use block flow (vertical stacking)
    layoutBlockFlow(layoutRoot, availableSpace);
}

std::unique_ptr<LayoutObject> LayoutEngine::createLayoutObject(const MarkdownObject* object, bool inCodeBlock) {
    switch (object->getType()) {
        case MarkdownObjectType::Document:
        case MarkdownObjectType::Paragraph:
        case MarkdownObjectType::Heading:
        case MarkdownObjectType::BlockQuote:
        case MarkdownObjectType::CodeBlock:
        case MarkdownObjectType::Frontmatter:
        case MarkdownObjectType::List:
        case MarkdownObjectType::ThematicBreak:
            return std::make_unique<BlockLayoutObject>(object);

        case MarkdownObjectType::ListItem:
            return std::make_unique<ListItemLayoutObject>(object);

        case MarkdownObjectType::Text: {
            auto textLayout = std::make_unique<TextLayoutObject>(object);
            textLayout->setFontProvider(fontProvider);
            if (inCodeBlock) {
                textLayout->setMonospace(true);
            }
            return textLayout;
        }

        case MarkdownObjectType::Image:
            return std::make_unique<ImageLayoutObject>(object);

        case MarkdownObjectType::Table:
            return std::make_unique<TableLayoutObject>(object);

        case MarkdownObjectType::TableRow:
            return std::make_unique<TableRowLayoutObject>(object);

        case MarkdownObjectType::TableCell:
            return std::make_unique<TableCellLayoutObject>(object);

        case MarkdownObjectType::Strong:
        case MarkdownObjectType::Emphasis:
        case MarkdownObjectType::InlineCode:
        case MarkdownObjectType::Link:
        case MarkdownObjectType::Strikethrough:
            // Inline formatting nodes are structural only - text is flattened with style ranges
            // Use base LayoutObject as a pass-through container for children
            return std::make_unique<LayoutObject>(object, LayoutFlow::Block);

        default:
            return std::make_unique<LayoutObject>(object, LayoutFlow::Block);
    }
}

void LayoutEngine::layoutBlockFlow(LayoutObject* layoutObject, const Size& availableSpace) {
    // Block flow layout - stack children vertically
    const MarkdownObject* sourceObj = layoutObject->getSourceObject();
    MarkdownObjectType type = sourceObj->getType();

    bool isRoot = (type == MarkdownObjectType::Document);
    bool isBlockQuote = (type == MarkdownObjectType::BlockQuote);
    bool isCodeBlock = (type == MarkdownObjectType::CodeBlock);
    bool isFrontmatter = (type == MarkdownObjectType::Frontmatter);
    bool isList = (type == MarkdownObjectType::List);

    // Block spacing should only be added between block-level children
    // Containers like Document, BlockQuote, and List contain block elements (paragraphs, etc.)
    // Containers like Paragraph and Heading contain inline elements (text) - no block spacing
    bool addBlockSpacing = isRoot || isBlockQuote || isList;

    float marginLeft = isRoot ? Typography::DOCUMENT_MARGIN : 0.0f;
    const float marginTop = isRoot ? Typography::DOCUMENT_MARGIN : 0.0f;

    // Blockquotes get extra left indent for the gray bar
    if (isBlockQuote) {
        marginLeft = Typography::BLOCKQUOTE_INDENT;
    }

    // Code blocks and frontmatter get small internal padding
    float codeBlockPadding = 0.0f;
    if (isCodeBlock || isFrontmatter) {
        marginLeft = Typography::CODE_BLOCK_PADDING;
        codeBlockPadding = 6.0f;
    }

    float currentY = marginTop + codeBlockPadding;
    bool isFirstVisibleChild = true;

    for (const auto& child : layoutObject->getChildren()) {
        const MarkdownObject* childSource = child->getSourceObject();
        MarkdownObjectType childType = childSource->getType();

        // Handle empty paragraphs - they need minimum height to be clickable
        if (childType == MarkdownObjectType::Paragraph) {
            std::string text = childSource->getText();
            // Check if paragraph is empty (only has empty text children)
            bool isEmpty = true;
            for (const auto& grandchild : childSource->getChildren()) {
                if (!grandchild->getText().empty()) {
                    isEmpty = false;
                    break;
                }
            }
            if (isEmpty) {
                // Visual mode: empty paragraphs are collapsed (0 height)
                // This matches browser behavior where empty lines between blocks
                // don't create visible space - spacing comes from block margins only.
                // The DOM positions still exist for cursor navigation.
                float emptyLineHeight = 0;
                Size childAvailable(availableSpace.width - marginLeft * 2, availableSpace.height - currentY);
                for (const auto& grandchild : child->getChildren()) {
                    grandchild->layout(childAvailable);
                    // Also collapse the TextLayoutObject's rect height
                    // so hitTest will skip it (it checks rect.size.height)
                    const Rect& grandchildRect = grandchild->getRect();
                    grandchild->setRect(Rect(grandchildRect.position.x, grandchildRect.position.y,
                                            grandchildRect.size.width, 0));
                }
                child->setRect(Rect(marginLeft, currentY, availableSpace.width - marginLeft * 2, emptyLineHeight));
                // Propagate position to children only from root (to avoid double-propagation)
                if (isRoot) {
                    propagatePositionToChildren(child.get(), marginLeft, currentY);
                }
                // Don't advance Y position - empty paragraphs take no visual space
                // Don't add block spacing for collapsed elements
                continue;
            }
        }

        Size childAvailableSpace(availableSpace.width - marginLeft * 2, availableSpace.height - currentY);

        // Unified layout authority: handle ListItem layout here, not in the object
        if (childType == MarkdownObjectType::ListItem) {
            layoutListItem(child.get(), childAvailableSpace);
        } else {
            performLayout(child.get(), childAvailableSpace);
        }

        const Rect& childRect = child->getRect();
        float childX = marginLeft;
        float childY = currentY;

        // Set child position
        child->setRect(Rect(childX, childY, childRect.size.width, childRect.size.height));

        // Only propagate positions from root - nested containers (List, BlockQuote)
        // don't need to propagate because the root will recursively propagate to all descendants
        if (isRoot) {
            propagatePositionToChildren(child.get(), childX, childY);
        }

        // Add spacing after this element (block spacing between elements)
        currentY += childRect.size.height;
        // Only add block spacing between block-level children (paragraphs, headings, etc.)
        // Not inside paragraphs/headings where children are inline text elements
        if (addBlockSpacing && (!isFirstVisibleChild || childRect.size.height > 0)) {
            currentY += Typography::BLOCK_SPACING;
        }
        isFirstVisibleChild = false;
    }

    // Add bottom padding for code blocks and frontmatter
    if (isCodeBlock || isFrontmatter) {
        currentY += codeBlockPadding;
    }

    layoutObject->setRect(Rect(0, 0, availableSpace.width, currentY));
}

void LayoutEngine::propagatePositionToChildren(LayoutObject* parent, float parentX, float parentY) {
    for (const auto& child : parent->getChildren()) {
        const Rect& childRect = child->getRect();
        // Add parent offset to get absolute position
        float absX = parentX + childRect.position.x;
        float absY = parentY + childRect.position.y;
        child->setRect(Rect(absX, absY, childRect.size.width, childRect.size.height));

        // Recurse for nested children
        propagatePositionToChildren(child.get(), absX, absY);
    }
}


// ============================================================================
// Unified Layout Authority: Table and List Layout
// The LayoutEngine is the sole coordinator of all positioning.
// Layout objects only report their intrinsic size - they never position
// themselves or their children. This eliminates split authority.
// ============================================================================

std::vector<float> LayoutEngine::computeTableColumnWidths(LayoutObject* table, float availableWidth) {
    const TableObject* tableObj = static_cast<const TableObject*>(table->getSourceObject());
    int columnCount = tableObj->getColumnCount();
    std::vector<float> columnWidths;

    if (columnCount == 0) return columnWidths;

    // Simple equal-width columns
    float borderWidth = 1.0f;
    float totalBorders = (columnCount + 1) * borderWidth;
    float usableWidth = availableWidth - totalBorders;
    float columnWidth = usableWidth / columnCount;

    for (int i = 0; i < columnCount; i++) {
        columnWidths.push_back(columnWidth);
    }
    return columnWidths;
}

void LayoutEngine::layoutTable(LayoutObject* table, const Size& availableSpace) {
    // Engine computes column widths and positions all rows
    std::vector<float> columnWidths = computeTableColumnWidths(table, availableSpace.width);

    float y = 0;
    float borderWidth = 1.0f;
    y += borderWidth;  // Top border

    for (auto& child : table->getChildren()) {
        // Engine positions table rows
        layoutTableRow(child.get(), columnWidths, availableSpace);

        const Rect& rowRect = child->getRect();
        // Position row - Engine is the sole authority
        // Don't propagate here - root will propagate to all descendants
        child->setRect(Rect(0, y, availableSpace.width, rowRect.size.height));

        y += rowRect.size.height + borderWidth;
    }

    table->setRect(Rect(0, 0, availableSpace.width, y));
}

void LayoutEngine::layoutTableRow(LayoutObject* row, const std::vector<float>& columnWidths, const Size& availableSpace) {
    float borderWidth = 1.0f;
    float cellPadding = 8.0f;

    float x = borderWidth;
    float maxHeight = 0;

    size_t colIndex = 0;
    for (auto& child : row->getChildren()) {
        if (colIndex >= columnWidths.size()) break;

        float cellWidth = columnWidths[colIndex];

        // Get cell alignment from source object
        TableCellAlign alignment = TableCellAlign::Left;
        if (TableCellLayoutObject* cellLayout = dynamic_cast<TableCellLayoutObject*>(child.get())) {
            alignment = cellLayout->getAlignment();
        }

        // Engine lays out cell content with alignment
        Size cellAvailable(cellWidth - cellPadding * 2, availableSpace.height);
        layoutTableCell(child.get(), alignment, cellAvailable);

        // Engine positions cell - sole authority
        const Rect& cellRect = child->getRect();
        child->setRect(Rect(x + cellPadding, cellPadding, cellWidth - cellPadding * 2, cellRect.size.height));

        maxHeight = std::max(maxHeight, cellRect.size.height + cellPadding * 2);

        x += cellWidth + borderWidth;
        colIndex++;
    }

    row->setRect(Rect(0, 0, availableSpace.width, maxHeight));
}

void LayoutEngine::layoutTableCell(LayoutObject* cell, TableCellAlign alignment, const Size& availableSpace) {
    float y = 0;

    // Layout children (text content)
    for (auto& child : cell->getChildren()) {
        performLayout(child.get(), availableSpace);
        const Rect& childRect = child->getRect();

        // Engine handles alignment - sole authority
        float childX = 0;
        if (alignment == TableCellAlign::Center) {
            childX = (availableSpace.width - childRect.size.width) / 2;
        } else if (alignment == TableCellAlign::Right) {
            childX = availableSpace.width - childRect.size.width;
        }

        // Engine positions content
        child->setRect(Rect(childX, y, childRect.size.width, childRect.size.height));
        y += childRect.size.height;
    }

    cell->setRect(Rect(0, 0, availableSpace.width, y));
}

void LayoutEngine::layoutListItem(LayoutObject* listItem, const Size& availableSpace) {
    // Get indent level from source object
    const ListItemObject* itemObj = static_cast<const ListItemObject*>(listItem->getSourceObject());
    int indent = itemObj->getIndentLevel();
    float indentWidth = Typography::LIST_INDENT * (indent + 1);  // +1 for base indent

    float y = 0;
    float contentWidth = availableSpace.width - indentWidth;

    // Layout children (text content)
    for (auto& child : listItem->getChildren()) {
        Size childAvailable(contentWidth, availableSpace.height - y);
        performLayout(child.get(), childAvailable);

        const Rect& childRect = child->getRect();
        // Engine positions content with indent - sole authority
        child->setRect(Rect(indentWidth, y, childRect.size.width, childRect.size.height));

        y += childRect.size.height;
    }

    listItem->setRect(Rect(0, 0, availableSpace.width, std::max(y, Typography::BASE_FONT_SIZE)));
}