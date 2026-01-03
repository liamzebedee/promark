#include "layout_engine.h"
#include "typography.h"
#include <iostream>

LayoutEngine::LayoutEngine() : fontFace(nullptr), monoFontFace(nullptr) {
}

LayoutEngine::~LayoutEngine() {
}

void LayoutEngine::setFontFace(FT_Face face) {
    fontFace = face;
}

void LayoutEngine::setMonoFontFace(FT_Face face) {
    monoFontFace = face;
}

FT_Face LayoutEngine::getFontFace() const {
    return fontFace;
}

FT_Face LayoutEngine::getMonoFontFace() const {
    return monoFontFace;
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

    // Handle table and list item layout specially - they manage their own child layout
    const MarkdownObject* sourceObj = layoutRoot->getSourceObject();
    if (sourceObj) {
        MarkdownObjectType type = sourceObj->getType();
        if (type == MarkdownObjectType::Table ||
            type == MarkdownObjectType::TableRow ||
            type == MarkdownObjectType::TableCell ||
            type == MarkdownObjectType::ListItem) {
            layoutRoot->layout(availableSpace);
            return;
        }
    }

    // Perform layout based on flow type
    if (layoutRoot->getFlow() == LayoutFlow::Block) {
        layoutBlockFlow(layoutRoot, availableSpace);
    } else {
        layoutInlineFlow(layoutRoot, availableSpace);
    }
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
            return std::make_unique<BlockLayoutObject>(object);

        case MarkdownObjectType::ListItem:
            return std::make_unique<ListItemLayoutObject>(object);

        case MarkdownObjectType::Text: {
            auto textLayout = std::make_unique<TextLayoutObject>(object);
            if (inCodeBlock && monoFontFace) {
                textLayout->setFontFace(monoFontFace);
                textLayout->setMonospace(true);
            } else {
                textLayout->setFontFace(fontFace);
                // Also provide mono font for inline code width calculations
                if (monoFontFace) {
                    textLayout->setMonoFontFace(monoFontFace);
                }
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

        case MarkdownObjectType::Bold:
        case MarkdownObjectType::Italic:
        case MarkdownObjectType::Underline:
        case MarkdownObjectType::Link:
            return std::make_unique<InlineLayoutObject>(object);

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

        // Skip empty paragraphs visually (they exist for cursor positioning but don't add space)
        if (childSource->getType() == MarkdownObjectType::Paragraph) {
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
                // Still layout it (for cursor positioning) but at zero height
                child->setRect(Rect(marginLeft, currentY, availableSpace.width - marginLeft * 2, 0));
                continue;
            }
        }

        Size childAvailableSpace(availableSpace.width - marginLeft * 2, availableSpace.height - currentY);
        performLayout(child.get(), childAvailableSpace);

        const Rect& childRect = child->getRect();
        float childX = marginLeft;
        float childY = currentY;

        // Set child position and propagate to grandchildren
        child->setRect(Rect(childX, childY, childRect.size.width, childRect.size.height));

        // For ListItem children, only propagate at document level to avoid double-propagation
        // (ListItem::layout sets children to relative positions, and we only want to convert
        // to absolute once, not both at List level AND Document level)
        MarkdownObjectType childType = childSource->getType();
        bool skipPropagate = (childType == MarkdownObjectType::ListItem && !isRoot);
        if (!skipPropagate) {
            propagatePositionToChildren(child.get(), childX, childY);
        }

        // Add spacing after this element (block spacing between elements)
        currentY += childRect.size.height;
        if (!isFirstVisibleChild || childRect.size.height > 0) {
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

void LayoutEngine::layoutInlineFlow(LayoutObject* layoutObject, const Size& availableSpace) {
    // TODO: Implement inline flow layout
    // - Flow children left-to-right
    // - Break lines when necessary
    // - Handle baseline alignment
    
    layoutObject->layout(availableSpace);
}