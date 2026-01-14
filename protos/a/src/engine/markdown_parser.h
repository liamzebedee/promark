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

    // Legacy annotation-based parsing (to be removed after tree model is complete)
    std::string parseInlineElements(const std::string& line, int lineRawStart, MarkdownObject* parent);

    // Tree-based inline parsing - creates structural inline children (Strong, Emphasis, etc.)
    // Per specs/01-document-model.md: "Formatting is Structural"
    void createInlineChildren(const std::string& text, MarkdownObject* parent, int rawStart = 0);

    // Table parsing
    void parseTableRow(const std::string& line, const std::vector<TableCellAlign>& alignments, MarkdownObject* row);

    // List parsing
    bool isListItem(const std::string& line);
    bool isOrderedListItem(const std::string& line);
    size_t parseListMarker(const std::string& line, ListMarkerType& type, std::string& marker);

    bool isHeading(const std::string& text, size_t position);
    bool isBlockQuote(const std::string& text, size_t position);
    bool isCodeBlock(const std::string& text, size_t position);
    bool isList(const std::string& text, size_t position);
};