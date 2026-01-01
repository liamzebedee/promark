#include "layout_engine.h"
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
    auto layoutObject = createLayoutObject(objectTree, inCodeBlock);

    // Create layout objects for children
    for (const auto& child : objectTree->getChildren()) {
        auto childLayout = createLayoutTree(child.get(), isCodeBlock || inCodeBlock);
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
        case MarkdownObjectType::List:
        case MarkdownObjectType::ListItem:
            return std::make_unique<BlockLayoutObject>(object);

        case MarkdownObjectType::Text: {
            auto textLayout = std::make_unique<TextLayoutObject>(object);
            if (inCodeBlock && monoFontFace) {
                textLayout->setFontFace(monoFontFace);
                textLayout->setMonospace(true);
            } else {
                textLayout->setFontFace(fontFace);
            }
            return textLayout;
        }

        case MarkdownObjectType::Image:
            return std::make_unique<ImageLayoutObject>(object);

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
    // Only root (Document) gets margins, children are positioned relative to parent
    bool isRoot = (layoutObject->getSourceObject()->getType() == MarkdownObjectType::Document);
    bool isBlockQuote = (layoutObject->getSourceObject()->getType() == MarkdownObjectType::BlockQuote);
    bool isCodeBlock = (layoutObject->getSourceObject()->getType() == MarkdownObjectType::CodeBlock);

    float marginLeft = isRoot ? 50.0f : 0.0f;
    const float marginTop = isRoot ? 50.0f : 0.0f;
    const float blockSpacing = 10.0f;

    // Blockquotes get extra left indent for the gray bar
    if (isBlockQuote) {
        marginLeft = 20.0f;  // Space for gray bar + padding
    }

    // Code blocks get small internal padding
    float codeBlockPadding = 0.0f;
    if (isCodeBlock) {
        marginLeft = 8.0f;
        codeBlockPadding = 6.0f;
    }

    float currentY = marginTop + codeBlockPadding;

    for (const auto& child : layoutObject->getChildren()) {
        Size childAvailableSpace(availableSpace.width - marginLeft * 2, availableSpace.height - currentY);
        performLayout(child.get(), childAvailableSpace);

        const Rect& childRect = child->getRect();
        float childX = marginLeft;
        float childY = currentY;

        // Set child position and propagate to grandchildren
        child->setRect(Rect(childX, childY, childRect.size.width, childRect.size.height));
        propagatePositionToChildren(child.get(), childX, childY);

        currentY += childRect.size.height + blockSpacing;
    }

    // Add bottom padding for code blocks
    if (isCodeBlock) {
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