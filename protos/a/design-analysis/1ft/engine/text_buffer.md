# TextBuffer Design Analysis

## Overview

`TextBuffer` is a minimal text storage abstraction that wraps `std::string` with insert/delete operations. Despite its name suggesting a sophisticated text editing data structure, it is a thin facade over a contiguous string buffer.

**Files:**
- `/home/liam/Documents/projects/promark/protos/a/src/engine/text_buffer.h` (17 lines)
- `/home/liam/Documents/projects/promark/protos/a/src/engine/text_buffer.cpp` (31 lines)

---

## 1. Responsibilities

### What This Module Must Do

1. **Store text content** - Hold the raw markdown text as a `std::string` (line 16, `.h`)
2. **Provide whole-buffer replacement** - `setText()` replaces entire content (lines 9-11, `.cpp`)
3. **Provide read access** - `getText()` returns const reference to content (lines 13-15, `.cpp`)
4. **Support positional insertion** - `insertText()` with bounds checking (lines 17-21, `.cpp`)
5. **Support positional deletion** - `deleteText()` with bounds checking (lines 23-27, `.cpp`)
6. **Report length** - `getLength()` for cursor bounds (lines 29-31, `.cpp`)

### What It Actually Does

The implementation is a transparent wrapper around `std::string`. Every method directly delegates to string operations:
- `setText` -> assignment
- `getText` -> return member
- `insertText` -> `std::string::insert`
- `deleteText` -> `std::string::erase`
- `getLength` -> `std::string::length`

---

## 2. Dependencies

### Header Dependencies (text_buffer.h)

| Line | Include | Purpose |
|------|---------|---------|
| 2 | `<string>` | Underlying storage type |

**Analysis:** Minimal dependency footprint. The module is self-contained with no engine-internal dependencies.

### Compile-time Dependencies (text_buffer.cpp)

| Line | Include | Purpose |
|------|---------|---------|
| 1 | `"text_buffer.h"` | Own header |

**Analysis:** No additional implementation dependencies. This is exemplary isolation.

### Consumers

The TextBuffer is consumed by:
1. **Engine** (`engine.h:65`) - Owns a `std::unique_ptr<TextBuffer>` as authoritative source
2. **MarkdownRenderer** (`markdown_renderer.h:63`) - Owns a *separate* `std::unique_ptr<TextBuffer>` copy
3. **MarkdownParser** (`markdown_parser.h:11`) - Accepts `const TextBuffer&` for parsing

---

## 3. Mutation Points

### Internal State

| Location | State | Mutators |
|----------|-------|----------|
| `.h:16` | `std::string buffer` | `setText`, `insertText`, `deleteText` |

### External Mutation Pattern (Critical Issue)

The TextBuffer is **never mutated through its own API** in practice. The actual mutation flow is:

```
Engine::inputBuffer (char*)  ──mutate──>  construct std::string  ──>  textBuffer->setText()  ──>  copy to MarkdownRenderer
```

Evidence from `engine.cpp`:

**Line 25-26:** Initial setup creates two independent TextBuffer instances:
```cpp
textBuffer = std::make_unique<TextBuffer>();
markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
```

**Lines 367-369, 391-392, 415-416, 428, 726, 763, 787, 808, 927, 1001, 1033, 1639, 1658, 1679, 1726, 1768, 1788:** Every mutation follows this pattern:
```cpp
std::string newText(inputBuffer, inputLength);
textBuffer->setText(newText);
markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
```

**Authority Problem:**
- The true authority is `Engine::inputBuffer` (a raw `char*` of 10MB, see `engine.h:52-53`)
- `TextBuffer` is a secondary copy, always derived from `inputBuffer`
- `MarkdownRenderer::textBuffer` is a tertiary copy, always derived from Engine's TextBuffer
- The `insertText()` and `deleteText()` methods on TextBuffer are **never called** anywhere in the codebase

---

## 4. Boundary Violations

### No Violations Present

The TextBuffer itself has no boundary violations. It imports only `<string>` and has no knowledge of:
- Rendering systems
- Parsing logic
- UI/input handling
- OpenGL/GLFW

### Violations in Consumers

The boundary violation exists in how consumers use TextBuffer:

**Engine owns cursor state that should belong to a text editing abstraction:**
- `engine.h:57-61`: `cursorPos`, `goalColumn`, `selectionStart`, `selectionEnd`, `hasSelection`

These are semantically part of "text buffer editing" but live in Engine because TextBuffer provides no cursor/selection support.

---

## 5. Declared-but-Unrealised Design

### Phantom API: insertText / deleteText

**Declaration (text_buffer.h:11-12):**
```cpp
void insertText(size_t position, const std::string& text);
void deleteText(size_t position, size_t length);
```

**Reality:** These methods are implemented but **never invoked**. A grep across the codebase shows zero call sites.

The Engine performs all text mutations directly on its `char* inputBuffer` using `memmove`/`memcpy`, then reconstructs the TextBuffer wholesale via `setText()`. Examples:

- `engine.cpp:359-360`: `memmove(inputBuffer + start, inputBuffer + end, ...)` then `textBuffer->setText(newText)`
- `engine.cpp:420-422`: `memmove(inputBuffer + cursorPos, inputBuffer + cursorPos + 1, ...)`
- `engine.cpp:754-757`: `memmove(...)` + `memcpy(...)` for paste operations

**Workaround Code:** The entire mutation pattern in Engine is a workaround. Instead of:
```cpp
textBuffer->insertText(cursorPos, text);
```
The code does:
```cpp
memmove(inputBuffer + cursorPos + insertLen, inputBuffer + cursorPos, ...);
memcpy(inputBuffer + cursorPos, text.c_str(), insertLen);
inputLength += insertLen;
std::string newText(inputBuffer, inputLength);
textBuffer->setText(newText);
markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
```

### Missing Copy Constructor Declaration

The code at `engine.cpp:26` and elsewhere uses:
```cpp
std::make_unique<TextBuffer>(*textBuffer)
```

This relies on the **implicit copy constructor** generated by the compiler. The TextBuffer class does not explicitly declare copy semantics, yet the entire synchronization mechanism depends on copying.

### Name Implies More Than Reality

The name "TextBuffer" suggests:
- A gap buffer, rope, or piece table (common text editor data structures)
- Efficient incremental editing operations
- Possibly undo/redo integration
- Line indexing or caching

In reality, it is `std::string` with a different name. The "Buffer" suffix implies buffer management that does not exist.

### Asymmetric Ownership Model

**MarkdownRenderer::setTextBuffer (markdown_renderer.h:28):**
```cpp
void setTextBuffer(std::unique_ptr<TextBuffer> buffer);
```

This takes ownership via `unique_ptr`, but the pattern of use is:
1. Engine creates a copy of its TextBuffer
2. Engine transfers ownership of the copy to MarkdownRenderer
3. MarkdownRenderer's old TextBuffer is destroyed
4. Repeat on every keystroke

This is a value-copy disguised as ownership transfer. The `unique_ptr` signature implies exclusive ownership semantics that don't match the actual data flow.

---

## Architectural Summary

| Concern | Status |
|---------|--------|
| **Single Responsibility** | Partial - stores text, but cursor state lives elsewhere |
| **API Utilization** | ~40% - insertText/deleteText are dead code |
| **Data Authority** | Broken - three copies exist, `char*` is authoritative |
| **Abstraction Value** | Minimal - could be replaced with `std::string` typedef |
| **Synchronization** | Manual, error-prone, performed 18+ times in engine.cpp |

### Recommendations for Future Work

1. **Consolidate authority** - Either TextBuffer owns the text or it doesn't exist
2. **Integrate cursor state** - Selection/cursor belong with the buffer being edited
3. **Remove dead API** - Delete `insertText`/`deleteText` or actually use them
4. **Share vs Copy** - Use `shared_ptr` or reference semantics instead of copying on every mutation
5. **Consider efficient structures** - If editing performance matters, implement gap buffer or piece table
