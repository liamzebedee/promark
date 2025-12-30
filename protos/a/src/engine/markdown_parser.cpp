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
    document->setRawRange(0, static_cast<int>(text.length()));

    size_t pos = 0;
    size_t textLen = text.length();

    while (pos < textLen) {
        // Find end of current line
        size_t lineStart = pos;
        size_t lineEnd = pos;
        while (lineEnd < textLen && text[lineEnd] != '\n') {
            lineEnd++;
        }

        std::string line = text.substr(lineStart, lineEnd - lineStart);
        size_t nextLineStart = (lineEnd < textLen) ? lineEnd + 1 : lineEnd;

        // Handle empty lines as empty paragraphs
        if (line.empty()) {
            auto emptyParagraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
            emptyParagraph->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));

            auto emptyText = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            emptyText->setText("");
            emptyText->setRawRange(static_cast<int>(lineStart), static_cast<int>(lineStart));
            emptyText->setTextOffset(0);
            emptyParagraph->addChild(std::move(emptyText));

            document->addChild(std::move(emptyParagraph));
            pos = nextLineStart;
            continue;
        }

        // Check if line is a heading
        if (line[0] == '#') {
            int level = 0;
            size_t syntaxEnd = 0;
            while (syntaxEnd < line.length() && line[syntaxEnd] == '#') {
                level++;
                syntaxEnd++;
            }

            // Skip space after #
            if (syntaxEnd < line.length() && line[syntaxEnd] == ' ') {
                syntaxEnd++;
            }

            // Get heading text - starts at lineStart + syntaxEnd in raw
            std::string headingText = line.substr(syntaxEnd);
            int textRawStart = static_cast<int>(lineStart + syntaxEnd);
            int textRawEnd = static_cast<int>(lineEnd);

            auto heading = std::make_unique<HeadingObject>(level);
            heading->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));

            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(headingText);
            textNode->setRawRange(textRawStart, textRawEnd);
            textNode->setTextOffset(0);  // Text starts at textRawStart

            heading->addChild(std::move(textNode));
            document->addChild(std::move(heading));
        } else {
            // Regular paragraph - no syntax stripping
            auto paragraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
            paragraph->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));

            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(line);
            textNode->setRawRange(static_cast<int>(lineStart), static_cast<int>(lineEnd));
            textNode->setTextOffset(0);

            paragraph->addChild(std::move(textNode));
            document->addChild(std::move(paragraph));
        }

        pos = nextLineStart;
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