#include "text_buffer.h"

TextBuffer::TextBuffer() : version(0), dirty(false) {
}

TextBuffer::~TextBuffer() {
}

void TextBuffer::incrementVersion() {
    version++;
    dirty = true;
}

void TextBuffer::setText(const std::string& text) {
    buffer = text;
    incrementVersion();
}

void TextBuffer::insertText(size_t position, const std::string& text) {
    if (position <= buffer.length()) {
        buffer.insert(position, text);
        incrementVersion();
    }
}

void TextBuffer::deleteText(size_t position, size_t length) {
    if (position < buffer.length()) {
        buffer.erase(position, length);
        incrementVersion();
    }
}

void TextBuffer::replaceText(size_t position, size_t length, const std::string& text) {
    if (position <= buffer.length()) {
        size_t deleteLen = std::min(length, buffer.length() - position);
        buffer.replace(position, deleteLen, text);
        incrementVersion();
    }
}

void TextBuffer::clear() {
    buffer.clear();
    incrementVersion();
}

const std::string& TextBuffer::getText() const {
    return buffer;
}

const char* TextBuffer::data() const {
    return buffer.data();
}

size_t TextBuffer::getLength() const {
    return buffer.length();
}

char TextBuffer::charAt(size_t position) const {
    if (position < buffer.length()) {
        return buffer[position];
    }
    return '\0';
}