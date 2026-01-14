#include "markdown_parser.h"
#include <sstream>
#include <iostream>

// Helper: collect display text from inline children (for layout compatibility)
// This allows the tree model to coexist with the current annotation-based layout/painting.
static std::string collectDisplayText(const MarkdownObject* obj) {
    std::string result;
    MarkdownObjectType type = obj->getType();

    switch (type) {
        case MarkdownObjectType::Text:
            return obj->getText();

        case MarkdownObjectType::LineBreak:
            return "\n";

        case MarkdownObjectType::Strong:
        case MarkdownObjectType::Emphasis:
        case MarkdownObjectType::InlineCode:
        case MarkdownObjectType::Link:
        case MarkdownObjectType::Strikethrough:
        case MarkdownObjectType::Paragraph:
        case MarkdownObjectType::Heading:
            // Recursively collect from children
            for (const auto& child : obj->getChildren()) {
                result += collectDisplayText(child.get());
            }
            return result;

        default:
            return "";
    }
}

// Helper: derive style ranges from tree structure (for layout/painting compatibility)
// Walks the inline tree and creates annotations based on node types.
static void buildStyleRangesFromTree(const MarkdownObject* obj, int& charPos,
                                     MarkdownObject* parent, bool inStrong, bool inEmphasis, bool inCode) {
    MarkdownObjectType type = obj->getType();

    switch (type) {
        case MarkdownObjectType::Text: {
            int len = static_cast<int>(obj->getText().length());
            if (len > 0 && (inStrong || inEmphasis || inCode)) {
                TextStyle style = TextStyle::Normal;
                if (inStrong && inEmphasis) {
                    style = TextStyle::BoldItalic;
                } else if (inStrong) {
                    style = TextStyle::Bold;
                } else if (inEmphasis) {
                    style = TextStyle::Italic;
                } else if (inCode) {
                    style = TextStyle::Code;
                }
                parent->addStyleRange(charPos, charPos + len, style);
            }
            charPos += len;
            break;
        }

        case MarkdownObjectType::LineBreak:
            charPos += 1;  // Newline character
            break;

        case MarkdownObjectType::Strong:
            for (const auto& child : obj->getChildren()) {
                buildStyleRangesFromTree(child.get(), charPos, parent, true, inEmphasis, inCode);
            }
            break;

        case MarkdownObjectType::Emphasis:
            for (const auto& child : obj->getChildren()) {
                buildStyleRangesFromTree(child.get(), charPos, parent, inStrong, true, inCode);
            }
            break;

        case MarkdownObjectType::InlineCode:
            for (const auto& child : obj->getChildren()) {
                buildStyleRangesFromTree(child.get(), charPos, parent, inStrong, inEmphasis, true);
            }
            break;

        case MarkdownObjectType::Link: {
            // Track link range
            int linkStart = charPos;
            for (const auto& child : obj->getChildren()) {
                buildStyleRangesFromTree(child.get(), charPos, parent, inStrong, inEmphasis, inCode);
            }
            int linkEnd = charPos;
            const LinkObject* linkObj = static_cast<const LinkObject*>(obj);
            parent->addLinkRange(linkStart, linkEnd, linkObj->getUrl());
            break;
        }

        default:
            // For other types, recurse into children
            for (const auto& child : obj->getChildren()) {
                buildStyleRangesFromTree(child.get(), charPos, parent, inStrong, inEmphasis, inCode);
            }
            break;
    }
}

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

        // Check for <br> tag (HTML line break)
        if (line[pos] == '<' && pos + 3 < line.length()) {
            // Check for <br>, <br/>, <br />
            std::string remaining = line.substr(pos);
            if (remaining.substr(0, 4) == "<br>" ||
                remaining.substr(0, 5) == "<br/>" ||
                remaining.substr(0, 6) == "<br />") {
                displayText += '\n';
                if (remaining.substr(0, 6) == "<br />") {
                    pos += 6;
                } else if (remaining.substr(0, 5) == "<br/>") {
                    pos += 5;
                } else {
                    pos += 4;
                }
                continue;
            }
        }

        // Check for inline code with backticks
        if (line[pos] == '`') {
            size_t endPos = line.find('`', pos + 1);
            if (endPos != std::string::npos) {
                std::string code = line.substr(pos + 1, endPos - pos - 1);
                int codeStart = static_cast<int>(displayText.length());
                displayText += code;
                int codeEnd = static_cast<int>(displayText.length());
                parent->addStyleRange(codeStart, codeEnd, TextStyle::Code);
                pos = endPos + 1;
                continue;
            }
        }

        displayText += line[pos];
        pos++;
    }

    return displayText;
}

// Tree-based inline parsing - creates structural inline children
// Per specs/01-document-model.md: "Formatting is Structural"
// "Hello **world**" becomes: Text("Hello "), Strong { Text("world") }
void MarkdownParser::createInlineChildren(const std::string& text, MarkdownObject* parent, int rawStart) {
    size_t pos = 0;
    size_t plainStart = 0;  // Start of current plain text run

    auto addPlainText = [&](size_t end) {
        if (end > plainStart) {
            std::string plainText = text.substr(plainStart, end - plainStart);
            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(plainText);
            textNode->setRawRange(rawStart + static_cast<int>(plainStart),
                                  rawStart + static_cast<int>(end));
            parent->addChild(std::move(textNode));
        }
    };

    while (pos < text.length()) {
        // Check for bold+italic (*** or ___) - creates nested Strong > Emphasis
        if (pos + 2 < text.length() &&
            ((text[pos] == '*' && text[pos+1] == '*' && text[pos+2] == '*') ||
             (text[pos] == '_' && text[pos+1] == '_' && text[pos+2] == '_'))) {
            char delim = text[pos];
            std::string delimStr(3, delim);
            size_t endPos = findClosingDelimiter(text, pos + 3, delimStr);
            if (endPos != std::string::npos) {
                // Add any preceding plain text
                addPlainText(pos);

                std::string content = text.substr(pos + 3, endPos - pos - 3);

                // Create nested Strong > Emphasis > Text
                auto strong = std::make_unique<StrongObject>();
                strong->setRawRange(rawStart + static_cast<int>(pos),
                                    rawStart + static_cast<int>(endPos + 3));

                auto emphasis = std::make_unique<EmphasisObject>();
                emphasis->setRawRange(rawStart + static_cast<int>(pos + 2),
                                      rawStart + static_cast<int>(endPos + 1));

                // Recursively parse content for nested inline formatting
                createInlineChildren(content, emphasis.get(), rawStart + static_cast<int>(pos + 3));

                strong->addChild(std::move(emphasis));
                parent->addChild(std::move(strong));

                pos = endPos + 3;
                plainStart = pos;
                continue;
            }
        }

        // Check for bold (** or __)
        if (pos + 1 < text.length() &&
            ((text[pos] == '*' && text[pos+1] == '*') ||
             (text[pos] == '_' && text[pos+1] == '_'))) {
            char delim = text[pos];
            std::string delimStr(2, delim);
            size_t endPos = findClosingDelimiter(text, pos + 2, delimStr);
            if (endPos != std::string::npos) {
                // Add any preceding plain text
                addPlainText(pos);

                std::string content = text.substr(pos + 2, endPos - pos - 2);

                auto strong = std::make_unique<StrongObject>();
                strong->setRawRange(rawStart + static_cast<int>(pos),
                                    rawStart + static_cast<int>(endPos + 2));

                // Recursively parse content for nested inline formatting
                createInlineChildren(content, strong.get(), rawStart + static_cast<int>(pos + 2));

                parent->addChild(std::move(strong));

                pos = endPos + 2;
                plainStart = pos;
                continue;
            }
        }

        // Check for italic (* or _)
        if (text[pos] == '*' || text[pos] == '_') {
            char delim = text[pos];
            std::string delimStr(1, delim);
            size_t endPos = findClosingDelimiter(text, pos + 1, delimStr);
            if (endPos != std::string::npos && endPos > pos + 1) {
                // Add any preceding plain text
                addPlainText(pos);

                std::string content = text.substr(pos + 1, endPos - pos - 1);

                auto emphasis = std::make_unique<EmphasisObject>();
                emphasis->setRawRange(rawStart + static_cast<int>(pos),
                                      rawStart + static_cast<int>(endPos + 1));

                // Recursively parse content for nested inline formatting
                createInlineChildren(content, emphasis.get(), rawStart + static_cast<int>(pos + 1));

                parent->addChild(std::move(emphasis));

                pos = endPos + 1;
                plainStart = pos;
                continue;
            }
        }

        // Check for link syntax [text](url)
        if (text[pos] == '[') {
            size_t textEnd = text.find(']', pos + 1);
            if (textEnd != std::string::npos && textEnd + 1 < text.length() && text[textEnd + 1] == '(') {
                size_t urlEnd = text.find(')', textEnd + 2);
                if (urlEnd != std::string::npos) {
                    // Add any preceding plain text
                    addPlainText(pos);

                    std::string linkText = text.substr(pos + 1, textEnd - pos - 1);
                    std::string url = text.substr(textEnd + 2, urlEnd - textEnd - 2);

                    auto link = std::make_unique<LinkObject>(url);
                    link->setRawRange(rawStart + static_cast<int>(pos),
                                      rawStart + static_cast<int>(urlEnd + 1));

                    // Create Text child for link text (links can contain inline formatting)
                    createInlineChildren(linkText, link.get(), rawStart + static_cast<int>(pos + 1));

                    parent->addChild(std::move(link));

                    pos = urlEnd + 1;
                    plainStart = pos;
                    continue;
                }
            }
        }

        // Check for <br> tag (HTML line break)
        if (text[pos] == '<' && pos + 3 < text.length()) {
            std::string remaining = text.substr(pos);
            size_t brLen = 0;
            if (remaining.substr(0, 6) == "<br />") {
                brLen = 6;
            } else if (remaining.substr(0, 5) == "<br/>") {
                brLen = 5;
            } else if (remaining.substr(0, 4) == "<br>") {
                brLen = 4;
            }
            if (brLen > 0) {
                // Add any preceding plain text
                addPlainText(pos);

                auto lineBreak = std::make_unique<LineBreakObject>();
                lineBreak->setRawRange(rawStart + static_cast<int>(pos),
                                       rawStart + static_cast<int>(pos + brLen));
                parent->addChild(std::move(lineBreak));

                pos += brLen;
                plainStart = pos;
                continue;
            }
        }

        // Check for inline code with backticks
        if (text[pos] == '`') {
            size_t endPos = text.find('`', pos + 1);
            if (endPos != std::string::npos) {
                // Add any preceding plain text
                addPlainText(pos);

                std::string code = text.substr(pos + 1, endPos - pos - 1);

                auto inlineCode = std::make_unique<InlineCodeObject>();
                inlineCode->setRawRange(rawStart + static_cast<int>(pos),
                                        rawStart + static_cast<int>(endPos + 1));

                // Code blocks don't have nested formatting - just plain text
                auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
                textNode->setText(code);
                textNode->setRawRange(rawStart + static_cast<int>(pos + 1),
                                      rawStart + static_cast<int>(endPos));
                inlineCode->addChild(std::move(textNode));

                parent->addChild(std::move(inlineCode));

                pos = endPos + 1;
                plainStart = pos;
                continue;
            }
        }

        pos++;
    }

    // Add any remaining plain text
    addPlainText(text.length());
}

std::unique_ptr<MarkdownObject> MarkdownParser::parseDocument(const std::string& text) {
    auto document = std::make_unique<MarkdownObject>(MarkdownObjectType::Document);
    document->setRawRange(0, static_cast<int>(text.length()));

    size_t pos = 0;
    size_t textLen = text.length();

    // Check for frontmatter at the very beginning of the document
    if (textLen >= 3 && text[0] == '-' && text[1] == '-' && text[2] == '-') {
        // Find the end of the opening --- line
        size_t openingLineEnd = 3;
        while (openingLineEnd < textLen && text[openingLineEnd] != '\n') {
            // Allow only whitespace after ---
            if (text[openingLineEnd] != ' ' && text[openingLineEnd] != '\t') {
                goto not_frontmatter;
            }
            openingLineEnd++;
        }

        // Skip past the newline
        size_t contentStart = (openingLineEnd < textLen) ? openingLineEnd + 1 : openingLineEnd;

        // Find closing ---
        size_t searchPos = contentStart;
        while (searchPos < textLen) {
            // Find start of line
            size_t lineStart = searchPos;
            size_t lineEnd = searchPos;
            while (lineEnd < textLen && text[lineEnd] != '\n') {
                lineEnd++;
            }

            std::string line = text.substr(lineStart, lineEnd - lineStart);

            // Check if this line is exactly --- (with optional trailing whitespace)
            if (line.length() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
                bool validClosing = true;
                for (size_t i = 3; i < line.length(); i++) {
                    if (line[i] != ' ' && line[i] != '\t') {
                        validClosing = false;
                        break;
                    }
                }

                if (validClosing) {
                    // Found valid closing delimiter
                    size_t closingLineEnd = (lineEnd < textLen) ? lineEnd + 1 : lineEnd;

                    // Extract frontmatter content (without trailing newline before closing)
                    std::string frontmatterContent;
                    if (lineStart > contentStart) {
                        frontmatterContent = text.substr(contentStart, lineStart - contentStart);
                        // Remove trailing newline if present
                        if (!frontmatterContent.empty() && frontmatterContent.back() == '\n') {
                            frontmatterContent.pop_back();
                        }
                    }

                    auto frontmatter = std::make_unique<FrontmatterObject>();
                    frontmatter->setContent(frontmatterContent);
                    frontmatter->setRawRange(0, static_cast<int>(closingLineEnd));

                    // Add text child for layout purposes
                    auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
                    textNode->setText(frontmatterContent);
                    textNode->setRawRange(static_cast<int>(contentStart), static_cast<int>(lineStart));
                    textNode->setTextOffset(0);
                    frontmatter->addChild(std::move(textNode));

                    document->addChild(std::move(frontmatter));
                    pos = closingLineEnd;
                    goto continue_parsing;
                }
            }

            searchPos = (lineEnd < textLen) ? lineEnd + 1 : lineEnd;
            if (searchPos <= lineEnd) break;  // Prevent infinite loop
        }
    }
not_frontmatter:
continue_parsing:

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

        // Check for thematic break (---, ***, ___)
        // Must have 3+ of the same character, with only optional spaces between
        if (line.length() >= 3 && (line[0] == '-' || line[0] == '*' || line[0] == '_')) {
            char breakChar = line[0];
            bool isThematicBreak = true;
            int charCount = 0;

            for (size_t i = 0; i < line.length(); i++) {
                if (line[i] == breakChar) {
                    charCount++;
                } else if (line[i] != ' ' && line[i] != '\t') {
                    // Non-matching, non-whitespace character
                    isThematicBreak = false;
                    break;
                }
            }

            if (isThematicBreak && charCount >= 3) {
                auto thematicBreak = std::make_unique<ThematicBreakObject>();
                thematicBreak->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));
                document->addChild(std::move(thematicBreak));
                pos = nextLineStart;
                continue;
            }
        }

        // Check for code block (```)
        if (line.length() >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`') {
            // Extract optional language identifier
            std::string language = line.substr(3);
            // Trim whitespace
            size_t start = language.find_first_not_of(" \t");
            size_t end = language.find_last_not_of(" \t");
            if (start != std::string::npos) {
                language = language.substr(start, end - start + 1);
            } else {
                language = "";
            }

            // Find closing ```
            size_t codeStart = nextLineStart;
            size_t codeEnd = codeStart;
            size_t closingLineEnd = codeStart;

            while (codeEnd < textLen) {
                // Find end of current line
                size_t scanLineStart = codeEnd;
                size_t scanLineEnd = codeEnd;
                while (scanLineEnd < textLen && text[scanLineEnd] != '\n') {
                    scanLineEnd++;
                }

                std::string scanLine = text.substr(scanLineStart, scanLineEnd - scanLineStart);

                // Check if this is the closing ```
                if (scanLine.length() >= 3 && scanLine[0] == '`' && scanLine[1] == '`' && scanLine[2] == '`') {
                    closingLineEnd = (scanLineEnd < textLen) ? scanLineEnd + 1 : scanLineEnd;
                    break;
                }

                codeEnd = (scanLineEnd < textLen) ? scanLineEnd + 1 : scanLineEnd;
                closingLineEnd = codeEnd;
            }

            // Extract code content (without trailing newline before closing ```)
            std::string codeContent;
            if (codeEnd > codeStart) {
                codeContent = text.substr(codeStart, codeEnd - codeStart);
                // Remove trailing newline if present
                if (!codeContent.empty() && codeContent.back() == '\n') {
                    codeContent.pop_back();
                }
            }

            auto codeBlock = std::make_unique<CodeBlockObject>(language);
            codeBlock->setCode(codeContent);
            codeBlock->setRawRange(static_cast<int>(lineStart), static_cast<int>(closingLineEnd));

            // Add a text child for layout purposes
            auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
            textNode->setText(codeContent);
            textNode->setRawRange(static_cast<int>(codeStart), static_cast<int>(codeEnd));
            textNode->setTextOffset(0);
            codeBlock->addChild(std::move(textNode));

            document->addChild(std::move(codeBlock));
            pos = closingLineEnd;
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

            auto heading = std::make_unique<HeadingObject>(level);
            heading->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));
            heading->setTextOffset(static_cast<int>(syntaxEnd));

            // Create inline children using tree-based model
            createInlineChildren(headingText, heading.get(), textRawStart);

            // For layout/painting compatibility: derive display text and annotations from tree
            std::string displayText = collectDisplayText(heading.get());
            heading->setText(displayText);
            int charPos = 0;
            buildStyleRangesFromTree(heading.get(), charPos, heading.get(), false, false, false);

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

            // Create inline children using tree-based model
            createInlineChildren(combinedText, paragraph.get(), static_cast<int>(firstTextRawStart));

            // For layout/painting compatibility: derive display text and annotations from tree
            std::string displayText = collectDisplayText(paragraph.get());
            paragraph->setText(displayText);
            int charPos = 0;
            buildStyleRangesFromTree(paragraph.get(), charPos, paragraph.get(), false, false, false);

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

            // Create inline children using tree-based model
            createInlineChildren(line, paragraph.get(), static_cast<int>(lineStart));

            // For layout/painting compatibility: derive display text and annotations from tree
            std::string displayText = collectDisplayText(paragraph.get());
            paragraph->setText(displayText);
            int charPos = 0;
            buildStyleRangesFromTree(paragraph.get(), charPos, paragraph.get(), false, false, false);

            document->addChild(std::move(paragraph));
        } else if (isListItem(line)) {
            // Parse list - collect all consecutive list items
            auto list = std::make_unique<ListObject>(isOrderedListItem(line));
            size_t listStart = lineStart;
            size_t listEnd = lineStart;

            size_t scanPos = pos;
            while (scanPos < textLen) {
                // Find the line at scanPos
                size_t scanLineStart = scanPos;
                size_t scanLineEnd = scanPos;
                while (scanLineEnd < textLen && text[scanLineEnd] != '\n') {
                    scanLineEnd++;
                }
                std::string scanLine = text.substr(scanLineStart, scanLineEnd - scanLineStart);

                // Check if this line is a list item or continuation
                if (scanLine.empty()) {
                    // Empty line ends list
                    break;
                }

                // Calculate indentation
                int indent = 0;
                size_t contentStart = 0;
                while (contentStart < scanLine.length() && (scanLine[contentStart] == ' ' || scanLine[contentStart] == '\t')) {
                    indent += (scanLine[contentStart] == '\t') ? 4 : 1;
                    contentStart++;
                }

                std::string trimmedLine = scanLine.substr(contentStart);

                if (isListItem(trimmedLine)) {
                    // Parse list item marker
                    ListMarkerType markerType;
                    std::string markerText;
                    size_t textStart = parseListMarker(trimmedLine, markerType, markerText);

                    std::string itemText = trimmedLine.substr(textStart);

                    auto listItem = std::make_unique<ListItemObject>();
                    listItem->setMarkerType(markerType);
                    listItem->setMarkerText(markerText);
                    listItem->setIndentLevel(indent / 2);  // 2 spaces per indent level
                    listItem->setRawRange(static_cast<int>(scanLineStart), static_cast<int>(scanLineEnd + 1));

                    // Parse inline elements in list item text
                    std::string displayText = parseInlineElements(itemText, static_cast<int>(scanLineStart + contentStart + textStart), listItem.get());

                    auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
                    textNode->setText(displayText);
                    textNode->setRawRange(static_cast<int>(scanLineStart + contentStart + textStart), static_cast<int>(scanLineEnd));
                    textNode->setTextOffset(0);
                    listItem->addChild(std::move(textNode));

                    list->addChild(std::move(listItem));
                    listEnd = scanLineEnd;

                    scanPos = (scanLineEnd < textLen) ? scanLineEnd + 1 : scanLineEnd;
                } else {
                    // Not a list item, stop
                    break;
                }
            }

            size_t listNextStart = (listEnd < textLen) ? listEnd + 1 : listEnd;
            list->setRawRange(static_cast<int>(listStart), static_cast<int>(listNextStart));
            document->addChild(std::move(list));
            pos = listNextStart;
            continue;
        } else if (line.length() >= 1 && line[0] == '|') {
            // Possible table - need to check for separator line
            // Look ahead to see if the next line is a separator (contains |---|)
            size_t nextNextLineStart = nextLineStart;
            size_t nextNextLineEnd = nextNextLineStart;
            while (nextNextLineEnd < textLen && text[nextNextLineEnd] != '\n') {
                nextNextLineEnd++;
            }
            std::string nextLine = text.substr(nextNextLineStart, nextNextLineEnd - nextNextLineStart);

            // Check if next line is a valid separator line
            bool isSeparator = false;
            std::vector<TableCellAlign> alignments;
            if (nextLine.length() >= 3 && nextLine[0] == '|') {
                isSeparator = true;
                // Parse separator line for alignments
                size_t sepPos = 1;
                while (sepPos < nextLine.length()) {
                    // Skip whitespace
                    while (sepPos < nextLine.length() && (nextLine[sepPos] == ' ' || nextLine[sepPos] == '\t')) {
                        sepPos++;
                    }
                    if (sepPos >= nextLine.length()) break;

                    // Check for alignment indicators
                    bool leftColon = (nextLine[sepPos] == ':');
                    if (leftColon) sepPos++;

                    // Skip dashes
                    size_t dashStart = sepPos;
                    while (sepPos < nextLine.length() && nextLine[sepPos] == '-') {
                        sepPos++;
                    }

                    if (sepPos == dashStart) {
                        // No dashes found - not a valid separator
                        if (nextLine[sepPos] == '|') {
                            sepPos++;
                            continue;
                        }
                        isSeparator = false;
                        break;
                    }

                    bool rightColon = (sepPos < nextLine.length() && nextLine[sepPos] == ':');
                    if (rightColon) sepPos++;

                    // Determine alignment
                    TableCellAlign align = TableCellAlign::Left;
                    if (leftColon && rightColon) {
                        align = TableCellAlign::Center;
                    } else if (rightColon) {
                        align = TableCellAlign::Right;
                    }
                    alignments.push_back(align);

                    // Skip whitespace
                    while (sepPos < nextLine.length() && (nextLine[sepPos] == ' ' || nextLine[sepPos] == '\t')) {
                        sepPos++;
                    }

                    // Expect pipe or end
                    if (sepPos < nextLine.length() && nextLine[sepPos] == '|') {
                        sepPos++;
                    }
                }
            }

            if (isSeparator && !alignments.empty()) {
                // This is a table! Parse it
                auto table = std::make_unique<TableObject>();
                table->setColumnAlignments(alignments);
                size_t tableStart = lineStart;

                // Parse header row (current line)
                auto headerRow = std::make_unique<TableRowObject>(true);
                parseTableRow(line, alignments, headerRow.get());
                table->addChild(std::move(headerRow));

                // Skip separator line
                size_t tablePos = (nextNextLineEnd < textLen) ? nextNextLineEnd + 1 : nextNextLineEnd;

                // Parse body rows
                while (tablePos < textLen) {
                    size_t rowLineStart = tablePos;
                    size_t rowLineEnd = tablePos;
                    while (rowLineEnd < textLen && text[rowLineEnd] != '\n') {
                        rowLineEnd++;
                    }
                    std::string rowLine = text.substr(rowLineStart, rowLineEnd - rowLineStart);

                    // Check if this is still a table row
                    if (rowLine.empty() || rowLine[0] != '|') {
                        break;
                    }

                    auto bodyRow = std::make_unique<TableRowObject>(false);
                    parseTableRow(rowLine, alignments, bodyRow.get());
                    table->addChild(std::move(bodyRow));

                    tablePos = (rowLineEnd < textLen) ? rowLineEnd + 1 : rowLineEnd;
                }

                table->setRawRange(static_cast<int>(tableStart), static_cast<int>(tablePos));
                document->addChild(std::move(table));
                pos = tablePos;
                continue;
            }

            // Not a valid table, treat as paragraph
            auto paragraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
            paragraph->setRawRange(static_cast<int>(lineStart), static_cast<int>(nextLineStart));

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

    // Handle trailing newline: if text ends with \n, create empty paragraph for cursor position
    // This allows the cursor to be placed on the new empty line after the final newline
    if (!text.empty() && text.back() == '\n') {
        auto emptyParagraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
        emptyParagraph->setRawRange(static_cast<int>(textLen), static_cast<int>(textLen));

        auto emptyText = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
        emptyText->setText("");
        emptyText->setRawRange(static_cast<int>(textLen), static_cast<int>(textLen));
        emptyText->setTextOffset(0);
        emptyParagraph->addChild(std::move(emptyText));

        document->addChild(std::move(emptyParagraph));
    }

    return document;
}

bool MarkdownParser::isListItem(const std::string& line) {
    if (line.empty()) return false;

    // Skip leading whitespace
    size_t pos = 0;
    while (pos < line.length() && (line[pos] == ' ' || line[pos] == '\t')) {
        pos++;
    }
    if (pos >= line.length()) return false;

    // Check for bullet markers: - or *
    if ((line[pos] == '-' || line[pos] == '*') && pos + 1 < line.length() && line[pos + 1] == ' ') {
        return true;
    }

    // Check for ordered list: number followed by . or )
    if (line[pos] >= '0' && line[pos] <= '9') {
        size_t numEnd = pos;
        while (numEnd < line.length() && line[numEnd] >= '0' && line[numEnd] <= '9') {
            numEnd++;
        }
        if (numEnd < line.length() && (line[numEnd] == '.' || line[numEnd] == ')')) {
            if (numEnd + 1 < line.length() && line[numEnd + 1] == ' ') {
                return true;
            }
        }
    }

    // Check for letter list: a. b. c. or A. B. C.
    if ((line[pos] >= 'a' && line[pos] <= 'z') || (line[pos] >= 'A' && line[pos] <= 'Z')) {
        if (pos + 1 < line.length() && (line[pos + 1] == '.' || line[pos + 1] == ')')) {
            if (pos + 2 < line.length() && line[pos + 2] == ' ') {
                return true;
            }
        }
    }

    return false;
}

bool MarkdownParser::isOrderedListItem(const std::string& line) {
    if (line.empty()) return false;

    // Skip leading whitespace
    size_t pos = 0;
    while (pos < line.length() && (line[pos] == ' ' || line[pos] == '\t')) {
        pos++;
    }
    if (pos >= line.length()) return false;

    // Check for ordered list: number followed by . or )
    if (line[pos] >= '0' && line[pos] <= '9') {
        return true;
    }

    // Check for letter list: a. b. c.
    if ((line[pos] >= 'a' && line[pos] <= 'z') || (line[pos] >= 'A' && line[pos] <= 'Z')) {
        if (pos + 1 < line.length() && (line[pos + 1] == '.' || line[pos + 1] == ')')) {
            return true;
        }
    }

    return false;
}

size_t MarkdownParser::parseListMarker(const std::string& line, ListMarkerType& type, std::string& marker) {
    size_t pos = 0;

    // Check for bullet markers: - or *
    if ((line[pos] == '-' || line[pos] == '*') && pos + 1 < line.length() && line[pos + 1] == ' ') {
        type = ListMarkerType::Bullet;
        marker = std::string(1, line[pos]);
        return 2;  // Skip marker and space
    }

    // Check for ordered list: number followed by . or )
    if (line[pos] >= '0' && line[pos] <= '9') {
        size_t numEnd = pos;
        while (numEnd < line.length() && line[numEnd] >= '0' && line[numEnd] <= '9') {
            numEnd++;
        }
        if (numEnd < line.length() && (line[numEnd] == '.' || line[numEnd] == ')')) {
            type = ListMarkerType::Number;
            marker = line.substr(pos, numEnd - pos + 1);  // e.g., "1."
            return numEnd + 2;  // Skip number, delimiter, and space
        }
    }

    // Check for letter list: a. b. c.
    if ((line[pos] >= 'a' && line[pos] <= 'z') || (line[pos] >= 'A' && line[pos] <= 'Z')) {
        if (pos + 1 < line.length() && (line[pos + 1] == '.' || line[pos + 1] == ')')) {
            type = ListMarkerType::Letter;
            marker = line.substr(pos, 2);  // e.g., "a."
            return 3;  // Skip letter, delimiter, and space
        }
    }

    type = ListMarkerType::Bullet;
    marker = "-";
    return 0;
}

void MarkdownParser::parseTableRow(const std::string& line, const std::vector<TableCellAlign>& alignments, MarkdownObject* row) {
    std::vector<std::string> cells;
    size_t pos = 0;

    // Skip leading pipe
    if (pos < line.length() && line[pos] == '|') {
        pos++;
    }

    // Parse cells
    std::string currentCell;
    while (pos < line.length()) {
        if (line[pos] == '|') {
            // Trim whitespace from cell content
            size_t start = currentCell.find_first_not_of(" \t");
            size_t end = currentCell.find_last_not_of(" \t");
            if (start != std::string::npos) {
                cells.push_back(currentCell.substr(start, end - start + 1));
            } else {
                cells.push_back("");
            }
            currentCell.clear();
        } else {
            currentCell += line[pos];
        }
        pos++;
    }

    // Add last cell if not empty (trailing content after last |)
    if (!currentCell.empty()) {
        size_t start = currentCell.find_first_not_of(" \t");
        size_t end = currentCell.find_last_not_of(" \t");
        if (start != std::string::npos) {
            cells.push_back(currentCell.substr(start, end - start + 1));
        }
    }

    // Create cell objects
    for (size_t i = 0; i < cells.size(); i++) {
        TableCellAlign align = (i < alignments.size()) ? alignments[i] : TableCellAlign::Left;
        auto cell = std::make_unique<TableCellObject>(align);

        // Parse inline elements in cell content
        std::string displayText = parseInlineElements(cells[i], 0, cell.get());

        auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
        textNode->setText(displayText);
        cell->addChild(std::move(textNode));

        row->addChild(std::move(cell));
    }

    // Pad with empty cells if needed
    while (row->getChildren().size() < alignments.size()) {
        size_t i = row->getChildren().size();
        TableCellAlign align = alignments[i];
        auto cell = std::make_unique<TableCellObject>(align);
        auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
        textNode->setText("");
        cell->addChild(std::move(textNode));
        row->addChild(std::move(cell));
    }
}