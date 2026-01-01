#pragma once
#include "markdown_objects.h"
#include "text_buffer.h"
#include <memory>

class MarkdownParser {
public:
    MarkdownParser();
    ~MarkdownParser();
    
    std::unique_ptr<MarkdownObject> parse(const TextBuffer& buffer);
    std::unique_ptr<MarkdownObject> parse(const std::string& markdown);
    
private:
    std::unique_ptr<MarkdownObject> parseDocument(const std::string& text);
    std::unique_ptr<MarkdownObject> parseBlock(const std::string& text, size_t& position);
    std::unique_ptr<MarkdownObject> parseInline(const std::string& text, size_t& position);
    std::string parseInlineElements(const std::string& line, int lineRawStart, MarkdownObject* parent);

    bool isHeading(const std::string& text, size_t position);
    bool isBlockQuote(const std::string& text, size_t position);
    bool isCodeBlock(const std::string& text, size_t position);
    bool isList(const std::string& text, size_t position);
};