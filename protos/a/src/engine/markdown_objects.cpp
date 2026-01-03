#include "markdown_objects.h"

MarkdownObject::MarkdownObject(MarkdownObjectType type) : type(type) {
}

MarkdownObject::~MarkdownObject() {
}

MarkdownObjectType MarkdownObject::getType() const {
    return type;
}

void MarkdownObject::addChild(std::unique_ptr<MarkdownObject> child) {
    children.push_back(std::move(child));
}

const std::vector<std::unique_ptr<MarkdownObject>>& MarkdownObject::getChildren() const {
    return children;
}

std::string MarkdownObject::getText() const {
    return text;
}

void MarkdownObject::setText(const std::string& text) {
    this->text = text;
}

void MarkdownObject::setRawRange(int start, int end) {
    rawStart = start;
    rawEnd = end;
}

void MarkdownObject::addLinkRange(int start, int end, const std::string& url) {
    linkRanges.push_back({start, end, url});
}

void MarkdownObject::addStyleRange(int start, int end, TextStyle style) {
    styleRanges.push_back({start, end, style});
}

HeadingObject::HeadingObject(int level) : MarkdownObject(MarkdownObjectType::Heading), level(level) {
}

int HeadingObject::getLevel() const {
    return level;
}

ImageObject::ImageObject(const std::string& src, const std::string& alt) 
    : MarkdownObject(MarkdownObjectType::Image), src(src), alt(alt) {
}

const std::string& ImageObject::getSrc() const {
    return src;
}

const std::string& ImageObject::getAlt() const {
    return alt;
}

LinkObject::LinkObject(const std::string& url)
    : MarkdownObject(MarkdownObjectType::Link), url(url) {
}

const std::string& LinkObject::getUrl() const {
    return url;
}

BlockQuoteObject::BlockQuoteObject()
    : MarkdownObject(MarkdownObjectType::BlockQuote) {
}

ListObject::ListObject(bool ordered)
    : MarkdownObject(MarkdownObjectType::List), ordered(ordered) {
}

bool ListObject::isOrdered() const {
    return ordered;
}

ListItemObject::ListItemObject()
    : MarkdownObject(MarkdownObjectType::ListItem) {
}

CodeBlockObject::CodeBlockObject(const std::string& language)
    : MarkdownObject(MarkdownObjectType::CodeBlock), language(language) {
}

const std::string& CodeBlockObject::getLanguage() const {
    return language;
}

const std::string& CodeBlockObject::getCode() const {
    return code;
}

void CodeBlockObject::setCode(const std::string& code) {
    this->code = code;
}

FrontmatterObject::FrontmatterObject()
    : MarkdownObject(MarkdownObjectType::Frontmatter) {
}

const std::string& FrontmatterObject::getContent() const {
    return content;
}

void FrontmatterObject::setContent(const std::string& content) {
    this->content = content;
}

TableObject::TableObject()
    : MarkdownObject(MarkdownObjectType::Table) {
}

void TableObject::setColumnAlignments(const std::vector<TableCellAlign>& alignments) {
    columnAlignments = alignments;
}

const std::vector<TableCellAlign>& TableObject::getColumnAlignments() const {
    return columnAlignments;
}

int TableObject::getColumnCount() const {
    return static_cast<int>(columnAlignments.size());
}

TableRowObject::TableRowObject(bool isHeader)
    : MarkdownObject(MarkdownObjectType::TableRow), header(isHeader) {
}

bool TableRowObject::isHeader() const {
    return header;
}

TableCellObject::TableCellObject(TableCellAlign align)
    : MarkdownObject(MarkdownObjectType::TableCell), alignment(align) {
}

TableCellAlign TableCellObject::getAlignment() const {
    return alignment;
}

void TableCellObject::setAlignment(TableCellAlign align) {
    alignment = align;
}