#include "layout_engine.h"

LayoutEngine::LayoutEngine() {
}

LayoutEngine::~LayoutEngine() {
}

std::unique_ptr<LayoutObject> LayoutEngine::createLayoutTree(const MarkdownObject* objectTree) {
    if (!objectTree) {
        return nullptr;
    }
    
    auto layoutObject = createLayoutObject(objectTree);
    
    // Create layout objects for children
    for (const auto& child : objectTree->getChildren()) {
        auto childLayout = createLayoutTree(child.get());
        if (childLayout) {
            layoutObject->addChild(std::move(childLayout));
        }
    }
    
    return layoutObject;
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

std::unique_ptr<LayoutObject> LayoutEngine::createLayoutObject(const MarkdownObject* object) {
    switch (object->getType()) {
        case MarkdownObjectType::Document:
        case MarkdownObjectType::Paragraph:
        case MarkdownObjectType::Heading:
        case MarkdownObjectType::BlockQuote:
        case MarkdownObjectType::CodeBlock:
        case MarkdownObjectType::List:
        case MarkdownObjectType::ListItem:
            return std::make_unique<BlockLayoutObject>(object);
            
        case MarkdownObjectType::Text:
            return std::make_unique<TextLayoutObject>(object);
            
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
    // TODO: Implement block flow layout
    // - Stack children vertically
    // - Each child gets full width, height determined by content
    float currentY = 0;
    
    for (const auto& child : layoutObject->getChildren()) {
        Size childAvailableSpace(availableSpace.width, availableSpace.height - currentY);
        performLayout(child.get(), childAvailableSpace);
        
        const Rect& childRect = child->getRect();
        child->setRect(Rect(0, currentY, childRect.size.width, childRect.size.height));
        currentY += childRect.size.height;
    }
    
    layoutObject->setRect(Rect(0, 0, availableSpace.width, currentY));
}

void LayoutEngine::layoutInlineFlow(LayoutObject* layoutObject, const Size& availableSpace) {
    // TODO: Implement inline flow layout
    // - Flow children left-to-right
    // - Break lines when necessary
    // - Handle baseline alignment
    
    layoutObject->layout(availableSpace);
}