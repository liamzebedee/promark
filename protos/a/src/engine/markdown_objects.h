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
    Equation,
    List,
    ListItem,
    Text
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

private:
    MarkdownObjectType type;
    std::vector<std::unique_ptr<MarkdownObject>> children;
    std::string text;
    int rawStart = 0;   // Start position in raw markdown
    int rawEnd = 0;     // End position in raw markdown
    int textOffset = 0; // Offset from rawStart to where visible text begins
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

class ListObject : public MarkdownObject {
public:
    ListObject(bool ordered);
    bool isOrdered() const;
    
private:
    bool ordered;
};