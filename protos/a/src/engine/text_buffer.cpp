#include "text_buffer.h"

TextBuffer::TextBuffer() {
}

TextBuffer::~TextBuffer() {
}

void TextBuffer::setText(const std::string& text) {
    buffer = text;
}

const std::string& TextBuffer::getText() const {
    return buffer;
}

void TextBuffer::insertText(size_t position, const std::string& text) {
    if (position <= buffer.length()) {
        buffer.insert(position, text);
    }
}

void TextBuffer::deleteText(size_t position, size_t length) {
    if (position < buffer.length()) {
        buffer.erase(position, length);
    }
}

size_t TextBuffer::getLength() const {
    return buffer.length();
}