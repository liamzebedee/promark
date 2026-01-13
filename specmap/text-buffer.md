# TextBuffer Component Specification

## 1. Purpose and Overview

The `TextBuffer` class provides a minimal abstraction layer over raw text storage for the promark markdown editor. It encapsulates a `std::string` buffer and exposes basic text manipulation operations.

**Key Responsibilities:**
- Store and retrieve document text content
- Provide position-based text insertion and deletion
- Report buffer length for boundary checking

**Design Philosophy:**
The TextBuffer is intentionally lightweight. It serves as a data container rather than a full-featured text editing component. The Engine class handles higher-level editing logic (cursor management, selection, undo/redo) while TextBuffer provides the underlying storage mechanism.

**Location:**
- Header: `/home/liam/Documents/projects/promark/protos/a/src/engine/text_buffer.h`
- Implementation: `/home/liam/Documents/projects/promark/protos/a/src/engine/text_buffer.cpp`

---

## 2. Buffer Data Structure

### Internal Storage

```cpp
class TextBuffer {
private:
    std::string buffer;
};
```

The buffer uses `std::string` for text storage, which provides:
- Dynamic memory allocation
- Automatic memory management
- UTF-8 byte sequence storage (though operations are byte-indexed, not Unicode-aware)
- Standard library compatibility

### Memory Characteristics

| Aspect | Behavior |
|--------|----------|
| Initial state | Empty string |
| Growth strategy | `std::string` default (typically amortized O(1) append) |
| Maximum size | Limited by `std::string::max_size()` and system memory |
| Encoding | Byte-based (UTF-8 compatible but not Unicode-aware) |

---

## 3. Text Editing Operations

### 3.1 setText(const std::string& text)

**Purpose:** Replace the entire buffer contents.

```cpp
void TextBuffer::setText(const std::string& text) {
    buffer = text;
}
```

**Behavior:**
- Completely replaces existing content
- No validation or sanitization
- O(n) complexity where n is text length

**Usage Context:** Called when loading a file or setting initial content via `Engine::setContent()`.

---

### 3.2 getText() const

**Purpose:** Retrieve the full buffer contents.

```cpp
const std::string& TextBuffer::getText() const {
    return buffer;
}
```

**Behavior:**
- Returns const reference (no copy)
- O(1) complexity

**Usage Context:** Used when saving files or syncing with the markdown parser.

---

### 3.3 insertText(size_t position, const std::string& text)

**Purpose:** Insert text at a specific byte position.

```cpp
void TextBuffer::insertText(size_t position, const std::string& text) {
    if (position <= buffer.length()) {
        buffer.insert(position, text);
    }
}
```

**Behavior:**
- Bounds check: position must be <= buffer length
- No-op if position is out of bounds
- Shifts existing content after insertion point
- O(n) complexity in worst case

**Edge Cases:**
- `position == buffer.length()`: Appends to end
- `position == 0`: Prepends to beginning
- `position > buffer.length()`: Silent no-op (no insertion)

---

### 3.4 deleteText(size_t position, size_t length)

**Purpose:** Remove a range of bytes from the buffer.

```cpp
void TextBuffer::deleteText(size_t position, size_t length) {
    if (position < buffer.length()) {
        buffer.erase(position, length);
    }
}
```

**Behavior:**
- Bounds check: position must be < buffer length
- `std::string::erase` handles length overflow gracefully (erases to end)
- No-op if position is out of bounds
- O(n) complexity

**Edge Cases:**
- `length` exceeding remaining content: Erases to end of buffer
- `position >= buffer.length()`: Silent no-op

---

### 3.5 getLength() const

**Purpose:** Return the buffer size in bytes.

```cpp
size_t TextBuffer::getLength() const {
    return buffer.length();
}
```

**Behavior:**
- Returns byte count, not character count (UTF-8 multi-byte sequences count as multiple)
- O(1) complexity

---

## 4. Selection Handling

**Note:** TextBuffer does not handle selection. Selection state is managed entirely by the Engine class.

### Engine Selection State

```cpp
// In Engine class
int selectionStart;
int selectionEnd;
bool hasSelection;
```

### Selection Behavior

| State | Description |
|-------|-------------|
| `hasSelection = false` | No active selection; cursor is a single insertion point |
| `hasSelection = true` | Active selection from `selectionStart` to `selectionEnd` |
| `selectionStart < selectionEnd` | Forward selection (left to right) |
| `selectionStart > selectionEnd` | Backward selection (right to left) |

### Selection-Aware Operations

The Engine uses selection state to modify TextBuffer content:

1. **Delete with selection:** Deletes selected range, then continues with normal operation
2. **Insert with selection:** Replaces selected range with new content
3. **Copy:** Extracts text from `min(start, end)` to `max(start, end)`

Example from Engine::insertChar():
```cpp
if (hasSelection) {
    int start = std::min(selectionStart, selectionEnd);
    int end = std::max(selectionStart, selectionEnd);
    memmove(inputBuffer + start, inputBuffer + end, inputLength - end + 1);
    inputLength -= (end - start);
    cursorPos = start;
    hasSelection = false;
}
```

---

## 5. Cursor Management

**Note:** Cursor position is not managed by TextBuffer. The Engine class maintains cursor state.

### Engine Cursor State

```cpp
// In Engine class
int cursorPos;      // Current cursor byte position
int goalColumn;     // Target column for vertical movement
```

### Cursor Operations

| Method | Description |
|--------|-------------|
| `moveCursor(delta, extendSelection)` | Move cursor left/right by delta bytes |
| `moveCursorByWord(direction, extendSelection)` | Jump to word boundaries |
| `moveCursorVertically(direction, extendSelection)` | Move up/down lines |
| `findLineStart(pos)` | Find start of line containing pos |
| `findLineEnd(pos)` | Find end of line containing pos |
| `getColumnInLine(pos)` | Get column offset within current line |
| `findPositionInLine(lineStart, column)` | Find position at given column |

### Goal Column Behavior

The `goalColumn` variable preserves horizontal position during vertical navigation:
- Updated when cursor moves horizontally
- Preserved when moving vertically through lines of varying length
- Allows cursor to return to original column when moving through shorter lines

---

## 6. Integration with Engine

### Dual Buffer Architecture

The Engine maintains two parallel text representations:

1. **inputBuffer (char*):** Primary editing buffer (10MB fixed allocation)
2. **textBuffer (TextBuffer):** Secondary buffer for markdown parsing

```cpp
// In Engine class
char* inputBuffer;
static const int INPUT_BUFFER_SIZE = 10 * 1024 * 1024;
int inputLength;

std::unique_ptr<TextBuffer> textBuffer;
std::unique_ptr<MarkdownRenderer> markdownRenderer;
```

### Synchronization Pattern

After any edit operation, the Engine syncs TextBuffer with inputBuffer:

```cpp
if (textBuffer && markdownRenderer) {
    std::string newText(inputBuffer, inputLength);
    textBuffer->setText(newText);
    markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
}
```

This pattern appears in:
- `insertChar()` / `insertText()`
- `deleteChar()` / `deleteWordBackward()`
- `paste()`
- `undo()`
- `setContent()`
- Backspace/Delete key handling
- Formatting operations (bold, italic, headings, links)

### Why Dual Buffers?

| Buffer | Purpose | Characteristics |
|--------|---------|-----------------|
| `inputBuffer` | Low-level editing | Fixed allocation, fast memmove operations |
| `TextBuffer` | Markdown parsing | std::string semantics, passed to renderer |

The fixed `inputBuffer` provides predictable memory behavior and efficient byte-level manipulation with `memmove()`. The `TextBuffer` provides a clean interface for the markdown rendering pipeline.

---

## 7. Notable Implementation Details

### 7.1 Copy Semantics

TextBuffer is copy-constructible (implicit compiler-generated copy constructor). The Engine exploits this when updating the markdown renderer:

```cpp
markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
```

This creates a fresh TextBuffer copy, ensuring the renderer has an independent snapshot.

### 7.2 Byte-Based Indexing

All positions are byte indices, not Unicode code points or grapheme clusters. This has implications:

- Multi-byte UTF-8 characters can be split if cursor lands mid-sequence
- Character counting differs from byte length for non-ASCII content
- Word boundary detection treats each byte as a unit

### 7.3 Silent Failure on Bounds Errors

Both `insertText` and `deleteText` silently ignore out-of-bounds operations rather than throwing exceptions or returning error codes. The Engine typically performs bounds checking before calling these methods.

### 7.4 No Undo Support

TextBuffer has no built-in undo mechanism. Undo is handled at the Engine level:

```cpp
struct UndoState {
    std::string text;
    int cursorPos;
};
std::vector<UndoState> undoStack;
```

The Engine captures full buffer snapshots before modifications.

### 7.5 Thread Safety

TextBuffer provides no thread safety guarantees. The single-threaded event loop design of the GLFW application means concurrent access is not expected.

### 7.6 Position Mapping

The markdown renderer introduces a concept of "DOM positions" that differ from raw buffer positions (e.g., markdown syntax characters may be hidden in rendered view). The Engine uses:

```cpp
int domCursorPos = markdownRenderer->rawToDOM(cursorPos);
```

This mapping is external to TextBuffer and handled by the MarkdownRenderer.

---

## API Summary

```cpp
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
```
