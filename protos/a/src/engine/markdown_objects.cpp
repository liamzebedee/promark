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