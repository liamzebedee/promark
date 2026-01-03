#pragma once
#include <vector>
#include <string>
#include <memory>

enum class MarkdownObjectType {
    Document,
    Heading,
    Paragraph,
    Image,
    Bold,
    Italic,
    Underline,
    Link,
    BlockQuote,
    CodeBlock,
    Frontmatter,
    Equation,
    List,
    ListItem,
    Table,
    TableRow,
    TableCell,
    Text
};

// Table cell alignment
enum class TableCellAlign {
    Left,
    Center,
    Right
};

// Inline link range within text
struct InlineLinkRange {
    int startChar;  // Start character index in display text
    int endChar;    // End character index in display text
    std::string url;
};

// Text style flags
enum class TextStyle : uint8_t {
    Normal = 0,
    Bold = 1 << 0,
    Italic = 1 << 1,
    Code = 1 << 2,
    BoldItalic = Bold | Italic
};

inline TextStyle operator|(TextStyle a, TextStyle b) {
    return static_cast<TextStyle>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool hasStyle(TextStyle style, TextStyle flag) {
    return (static_cast<uint8_t>(style) & static_cast<uint8_t>(flag)) != 0;
}

// Inline style range within text
struct InlineStyleRange {
    int startChar;  // Start character index in display text
    int endChar;    // End character index in display text
    TextStyle style;
};

class MarkdownObject {
public:
    MarkdownObject(MarkdownObjectType type);
    virtual ~MarkdownObject();

    MarkdownObjectType getType() const;
    void addChild(std::unique_ptr<MarkdownObject> child);
    const std::vector<std::unique_ptr<MarkdownObject>>& getChildren() const;

    virtual std::string getText() const;
    virtual void setText(const std::string& text);

    // Raw position tracking for DOM↔raw mapping
    void setRawRange(int start, int end);
    int getRawStart() const { return rawStart; }
    int getRawEnd() const { return rawEnd; }
    // Offset within this object where text content begins (after syntax like "# ")
    void setTextOffset(int offset) { textOffset = offset; }
    int getTextOffset() const { return textOffset; }

    // Inline link ranges (for paragraphs with links)
    void addLinkRange(int start, int end, const std::string& url);
    const std::vector<InlineLinkRange>& getLinkRanges() const { return linkRanges; }

    // Inline style ranges (for bold/italic text)
    void addStyleRange(int start, int end, TextStyle style);
    const std::vector<InlineStyleRange>& getStyleRanges() const { return styleRanges; }

private:
    MarkdownObjectType type;
    std::vector<std::unique_ptr<MarkdownObject>> children;
    std::string text;
    int rawStart = 0;   // Start position in raw markdown
    int rawEnd = 0;     // End position in raw markdown
    int textOffset = 0; // Offset from rawStart to where visible text begins
    std::vector<InlineLinkRange> linkRanges;
    std::vector<InlineStyleRange> styleRanges;
};

class HeadingObject : public MarkdownObject {
public:
    HeadingObject(int level);
    int getLevel() const;
    
private:
    int level;
};

class ImageObject : public MarkdownObject {
public:
    ImageObject(const std::string& src, const std::string& alt);
    const std::string& getSrc() const;
    const std::string& getAlt() const;
    
private:
    std::string src;
    std::string alt;
};

class LinkObject : public MarkdownObject {
public:
    LinkObject(const std::string& url);
    const std::string& getUrl() const;
    
private:
    std::string url;
};

class BlockQuoteObject : public MarkdownObject {
public:
    BlockQuoteObject();
};

// List marker type
enum class ListMarkerType {
    Bullet,     // - or *
    Number,     // 1. 2. 3.
    Letter      // a. b. c. or A. B. C.
};

class ListObject : public MarkdownObject {
public:
    ListObject(bool ordered);
    bool isOrdered() const;
    void setIndentLevel(int level) { indentLevel = level; }
    int getIndentLevel() const { return indentLevel; }

private:
    bool ordered;
    int indentLevel = 0;
};

class ListItemObject : public MarkdownObject {
public:
    ListItemObject();
    void setMarkerType(ListMarkerType type) { markerType = type; }
    ListMarkerType getMarkerType() const { return markerType; }
    void setMarkerText(const std::string& text) { markerText = text; }
    const std::string& getMarkerText() const { return markerText; }
    void setIndentLevel(int level) { indentLevel = level; }
    int getIndentLevel() const { return indentLevel; }

private:
    ListMarkerType markerType = ListMarkerType::Bullet;
    std::string markerText = "-";  // The actual marker (-, *, 1., a., etc.)
    int indentLevel = 0;
};

class CodeBlockObject : public MarkdownObject {
public:
    CodeBlockObject(const std::string& language = "");
    const std::string& getLanguage() const;
    const std::string& getCode() const;
    void setCode(const std::string& code);

private:
    std::string language;
    std::string code;
};

class FrontmatterObject : public MarkdownObject {
public:
    FrontmatterObject();
    const std::string& getContent() const;
    void setContent(const std::string& content);

private:
    std::string content;
};

class TableObject : public MarkdownObject {
public:
    TableObject();
    void setColumnAlignments(const std::vector<TableCellAlign>& alignments);
    const std::vector<TableCellAlign>& getColumnAlignments() const;
    int getColumnCount() const;

private:
    std::vector<TableCellAlign> columnAlignments;
};

class TableRowObject : public MarkdownObject {
public:
    TableRowObject(bool isHeader = false);
    bool isHeader() const;

private:
    bool header;
};

class TableCellObject : public MarkdownObject {
public:
    TableCellObject(TableCellAlign align = TableCellAlign::Left);
    TableCellAlign getAlignment() const;
    void setAlignment(TableCellAlign align);

private:
    TableCellAlign alignment;
};