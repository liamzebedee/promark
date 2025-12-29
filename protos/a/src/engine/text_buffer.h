#pragma once
#include <string>

class TextBuffer {
public:
    TextBuffer();
    ~TextBuffer();
    
    void setText(const std::string& text);
    const std::string& getText() const;
    void insertText(size_t position, const std::string& text);
    void deleteText(size_t position, size_t length);
    size_t getLength() const;
    
private:
    std::string buffer;
};