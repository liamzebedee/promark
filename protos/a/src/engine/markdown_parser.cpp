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

// Helper to find closing delimiter for bold/italic
// Returns position after the closing delimiter, or std::string::npos if not found
static size_t findClosingDelimiter(const std::string& line, size_t start, const std::string& delim) {
    size_t pos = start;
    while (pos < line.length()) {
        size_t found = line.find(delim, pos);
        if (found == std::string::npos) {
            return std::string::npos;
        }
        // Make sure it's not escaped and not at word boundary issues
        // For simplicity, just find the delimiter
        return found;
    }
    return std::string::npos;
}

// Parse inline elements like links [text](url), **bold**, *italic* within a line
// Returns the display text (with syntax removed) and populates ranges on parent
std::string MarkdownParser::parseInlineElements(const std::string& line, int lineRawStart,
                                                 MarkdownObject* parent) {
    (void)lineRawStart;  // Not used currently
    std::string displayText;
    size_t pos = 0;

    while (pos < line.length()) {
        // Check for bold+italic (*** or ___)
        if (pos + 2 < line.length() &&
            ((line[pos] == '*' && line[pos+1] == '*' && line[pos+2] == '*') ||
             (line[pos] == '_' && line[pos+1] == '_' && line[pos+2] == '_'))) {
            char delim = line[pos];
            std::string delimStr(3, delim);
            size_t endPos = findClosingDelimiter(line, pos + 3, delimStr);
            if (endPos != std::string::npos) {
                std::string content = line.substr(pos + 3, endPos - pos - 3);
                int styleStart = static_cast<int>(displayText.length());
                displayText += content;
                int styleEnd = static_cast<int>(displayText.length());
                parent->addStyleRange(styleStart, styleEnd, TextStyle::BoldItalic);
                pos = endPos + 3;
                continue;
            }
        }

        // Check for bold (** or __)
        if (pos + 1 < line.length() &&
            ((line[pos] == '*' && line[pos+1] == '*') ||
             (line[pos] == '_' && line[pos+1] == '_'))) {
            char delim = line[pos];
            std::string delimStr(2, delim);
            size_t endPos = findClosingDelimiter(line, pos + 2, delimStr);
            if (endPos != std::string::npos) {
                std::string content = line.substr(pos + 2, endPos - pos - 2);
                int styleStart = static_cast<int>(displayText.length());
                displayText += content;
                int styleEnd = static_cast<int>(displayText.length());
                parent->addStyleRange(styleStart, styleEnd, TextStyle::Bold);
                pos = endPos + 2;
                continue;
            }
        }

        // Check for italic (* or _)
        if (line[pos] == '*' || line[pos] == '_') {
            char delim = line[pos];
            std::string delimStr(1, delim);
            size_t endPos = findClosingDelimiter(line, pos + 1, delimStr);
            if (endPos != std::string::npos && endPos > pos + 1) {
                std::string content = line.substr(pos + 1, endPos - pos - 1);
                int styleStart = static_cast<int>(displayText.length());
                displayText += content;
                int styleEnd = static_cast<int>(displayText.length());
                parent->addStyleRange(styleStart, styleEnd, TextStyle::Italic);
                pos = endPos + 1;
                continue;
            }
        }

        // Check for link syntax [text](url)
        if (line[pos] == '[') {
            size_t textEnd = line.find(']', pos + 1);
            if (textEnd != std::string::npos && textEnd + 1 < line.length() && line[textEnd + 1] == '(') {
                size_t urlEnd = line.find(')', textEnd + 2);
                if (urlEnd != std::string::npos) {
                    // Extract link text and URL
                    std::string linkText = line.substr(pos + 1, textEnd - pos - 1);
                    std::string url = line.substr(textEnd + 2, urlEnd - textEnd - 2);

                    // Track link range in display text
                    int linkStart = static_cast<int>(displayText.length());
                    displayText += linkText;
                    int linkEnd = static_cast<int>(displayText.length());

                    parent->addLinkRange(linkStart, linkEnd, url);

                    pos = urlEnd + 1;
                    continue;
                }
            }
        }

        displayText += line[pos];
        pos++;
    }

    return displayText;
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

            // Parse inline elements (bold, italic, links) in heading text
            std::string displayText = parseInlineElements(headingText, textRawStart, heading.get());

            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(displayText);
            textNode->setRawRange(textRawStart, textRawEnd);
            textNode->setTextOffset(0);  // Text starts at textRawStart

            heading->addChild(std::move(textNode));
            document->addChild(std::move(heading));
        } else if (line[0] == '>') {
            // Block quote - collect consecutive > lines into a single block
            size_t blockStart = lineStart;
            std::string combinedText;
            size_t blockEnd = lineEnd;
            size_t firstTextRawStart = 0;

            // Collect all consecutive blockquote lines
            size_t scanPos = pos;
            bool firstLine = true;
            while (scanPos < textLen) {
                // Find the line at scanPos
                size_t scanLineStart = scanPos;
                size_t scanLineEnd = scanPos;
                while (scanLineEnd < textLen && text[scanLineEnd] != '\n') {
                    scanLineEnd++;
                }
                std::string scanLine = text.substr(scanLineStart, scanLineEnd - scanLineStart);

                // Check if this line is a blockquote
                if (!scanLine.empty() && scanLine[0] == '>') {
                    // Extract text after "> "
                    size_t syntaxEnd = 1;
                    if (syntaxEnd < scanLine.length() && scanLine[syntaxEnd] == ' ') {
                        syntaxEnd++;
                    }
                    std::string lineText = scanLine.substr(syntaxEnd);

                    if (firstLine) {
                        firstTextRawStart = scanLineStart + syntaxEnd;
                        firstLine = false;
                    } else {
                        combinedText += '\n';
                    }
                    combinedText += lineText;
                    blockEnd = scanLineEnd;

                    // Move to next line
                    scanPos = (scanLineEnd < textLen) ? scanLineEnd + 1 : scanLineEnd;
                } else {
                    // Not a blockquote line, stop collecting
                    break;
                }
            }

            size_t blockNextStart = (blockEnd < textLen) ? blockEnd + 1 : blockEnd;

            auto blockquote = std::make_unique<BlockQuoteObject>();
            blockquote->setRawRange(static_cast<int>(blockStart), static_cast<int>(blockNextStart));

            auto paragraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
            paragraph->setRawRange(static_cast<int>(firstTextRawStart), static_cast<int>(blockEnd));

            // Parse inline elements (bold, italic, links) in blockquote text
            std::string displayText = parseInlineElements(combinedText, static_cast<int>(firstTextRawStart), paragraph.get());

            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(displayText);
            textNode->setRawRange(static_cast<int>(firstTextRawStart), static_cast<int>(blockEnd));
            textNode->setTextOffset(0);
            paragraph->addChild(std::move(textNode));

            blockquote->addChild(std::move(paragraph));
            document->addChild(std::move(blockquote));

            // Skip to after the block
            pos = blockNextStart;
            continue;
        } else if (line.length() >= 4 && line[0] == '!' && line[1] == '[') {
            // Check for image syntax: ![alt](src)
            size_t altEnd = line.find(']', 2);
            if (altEnd != std::string::npos && altEnd + 1 < line.length() && line[altEnd + 1] == '(') {
                size_t srcEnd = line.find(')', altEnd + 2);
                if (srcEnd != std::string::npos) {
                    // Extract alt text and source
                    std::string altText = line.substr(2, altEnd - 2);
                    std::string src = line.substr(altEnd + 2, srcEnd - altEnd - 2);

                    auto image = std::make_unique<ImageObject>(src, altText);
                    image->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));
                    document->addChild(std::move(image));
                    pos = nextLineStart;
                    continue;
                }
            }
            // If image syntax is incomplete, treat as regular paragraph
            auto paragraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
            paragraph->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));

            // Parse inline elements (bold, italic, links)
            std::string displayText = parseInlineElements(line, static_cast<int>(lineStart), paragraph.get());

            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(displayText);
            textNode->setRawRange(static_cast<int>(lineStart), static_cast<int>(lineEnd));
            textNode->setTextOffset(0);

            paragraph->addChild(std::move(textNode));
            document->addChild(std::move(paragraph));
        } else {
            // Regular paragraph - parse inline elements (links)
            auto paragraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
            paragraph->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));

            std::string displayText = parseInlineElements(line, static_cast<int>(lineStart), paragraph.get());

            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(displayText);
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