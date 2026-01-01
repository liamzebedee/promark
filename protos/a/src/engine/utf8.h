#pragma once
#include <string>
#include <vector>
#include <cstdint>

// UTF-8 utilities for decoding to Unicode code points

namespace utf8 {

// Decode a single UTF-8 character starting at pos, return code point and advance pos
inline uint32_t decode(const std::string& str, size_t& pos) {
    if (pos >= str.length()) {
        return 0;
    }

    uint8_t c = static_cast<uint8_t>(str[pos]);

    // ASCII (0xxxxxxx)
    if ((c & 0x80) == 0) {
        pos++;
        return c;
    }

    uint32_t codepoint = 0;
    size_t numBytes = 0;

    // 2-byte sequence (110xxxxx)
    if ((c & 0xE0) == 0xC0) {
        codepoint = c & 0x1F;
        numBytes = 2;
    }
    // 3-byte sequence (1110xxxx)
    else if ((c & 0xF0) == 0xE0) {
        codepoint = c & 0x0F;
        numBytes = 3;
    }
    // 4-byte sequence (11110xxx)
    else if ((c & 0xF8) == 0xF0) {
        codepoint = c & 0x07;
        numBytes = 4;
    }
    // Invalid UTF-8, treat as single byte
    else {
        pos++;
        return c;
    }

    // Read continuation bytes (10xxxxxx)
    for (size_t i = 1; i < numBytes && pos + i < str.length(); i++) {
        uint8_t cont = static_cast<uint8_t>(str[pos + i]);
        if ((cont & 0xC0) != 0x80) {
            // Invalid continuation byte, return what we have
            pos++;
            return c;
        }
        codepoint = (codepoint << 6) | (cont & 0x3F);
    }

    pos += numBytes;
    return codepoint;
}

// Decode entire string to vector of code points
inline std::vector<uint32_t> decode(const std::string& str) {
    std::vector<uint32_t> codepoints;
    codepoints.reserve(str.length());  // At most this many

    size_t pos = 0;
    while (pos < str.length()) {
        codepoints.push_back(decode(str, pos));
    }

    return codepoints;
}

// Get the number of code points in a UTF-8 string
inline size_t length(const std::string& str) {
    size_t count = 0;
    size_t pos = 0;
    while (pos < str.length()) {
        decode(str, pos);
        count++;
    }
    return count;
}

// Get byte offset for the nth code point
inline size_t byteOffset(const std::string& str, size_t charIndex) {
    size_t pos = 0;
    size_t count = 0;
    while (pos < str.length() && count < charIndex) {
        decode(str, pos);
        count++;
    }
    return pos;
}

}  // namespace utf8
