#pragma once
#include <string>
#include <cstdint>

// TextBuffer is the single source of truth for document content.
// All text mutations must go through this class.
// Downstream consumers receive read-only access via const reference.
class TextBuffer {
public:
    TextBuffer();
    ~TextBuffer();

    // Content mutation methods - all mutations go through these
    void setText(const std::string& text);
    void insertText(size_t position, const std::string& text);
    void deleteText(size_t position, size_t length);
    void replaceText(size_t position, size_t length, const std::string& text);
    void clear();

    // Read-only access - downstream consumers use these
    const std::string& getText() const;
    const char* data() const;
    size_t getLength() const;
    char charAt(size_t position) const;

    // Version tracking for cache invalidation
    // Version increments on every mutation, enabling downstream caches
    // to efficiently detect when re-parsing/re-layout is needed
    uint64_t getVersion() const { return version; }

    // Dirty flag for save state tracking
    bool isDirty() const { return dirty; }
    void markClean() { dirty = false; }

private:
    std::string buffer;
    uint64_t version;  // Increments on every mutation
    bool dirty;        // True if modified since last save

    void incrementVersion();
};