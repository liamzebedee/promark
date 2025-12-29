#include "markdown_parser.h"
#include <sstream>
#include <iostream>

MarkdownParser::MarkdownParser() {
}

MarkdownParser::~MarkdownParser() {
}

std::unique_ptr<MarkdownObject> MarkdownParser::parse(const TextBuffer& buffer) {
    return parse(buffer.getText());
}

std::unique_ptr<MarkdownObject> MarkdownParser::parse(const std::string& markdown) {
    return parseDocument(markdown);
}

std::unique_ptr<MarkdownObject> MarkdownParser::parseDocument(const std::string& text) {
    auto document = std::make_unique<MarkdownObject>(MarkdownObjectType::Document);
    
    std::string line;
    std::istringstream stream(text);
    
    while (std::getline(stream, line)) {
        
        // Skip empty lines
        if (line.empty()) {
            continue;
        }
        
        // Check if line is a heading
        if (line[0] == '#') {
            int level = 0;
            size_t pos = 0;
            while (pos < line.length() && line[pos] == '#') {
                level++;
                pos++;
            }
            
            // Skip space after #
            if (pos < line.length() && line[pos] == ' ') {
                pos++;
            }
            
            // Get heading text
            std::string headingText = line.substr(pos);
            
            auto heading = std::make_unique<HeadingObject>(level);
            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(headingText);
            heading->addChild(std::move(textNode));
            document->addChild(std::move(heading));
        } else {
            // Regular paragraph
            auto paragraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(line);
            paragraph->addChild(std::move(textNode));
            document->addChild(std::move(paragraph));
        }
    }
    
    return document;
}

std::unique_ptr<MarkdownObject> MarkdownParser::parseBlock(const std::string& text, size_t& position) {
    // TODO: Implement block parsing
    return nullptr;
}

std::unique_ptr<MarkdownObject> MarkdownParser::parseInline(const std::string& text, size_t& position) {
    // TODO: Implement inline parsing
    return nullptr;
}

bool MarkdownParser::isHeading(const std::string& text, size_t position) {
    // TODO: Implement heading detection
    return false;
}

bool MarkdownParser::isBlockQuote(const std::string& text, size_t position) {
    // TODO: Implement blockquote detection
    return false;
}

bool MarkdownParser::isCodeBlock(const std::string& text, size_t position) {
    // TODO: Implement code block detection
    return false;
}

bool MarkdownParser::isList(const std::string& text, size_t position) {
    // TODO: Implement list detection
    return false;
}