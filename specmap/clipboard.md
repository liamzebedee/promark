# Clipboard Component Specification

## 1. Purpose and Overview

The Clipboard component provides a **platform-agnostic interface** for clipboard operations (copy/paste) in the Promark markdown editor. It abstracts away platform-specific clipboard APIs, allowing the same editor codebase to work across different environments:

- **Desktop (GLFW)**: Uses native GLFW clipboard functions
- **Web/Custom**: Supports custom callback handlers for alternative implementations

This design enables the editor to be compiled for web targets (e.g., WebAssembly) where browser clipboard APIs differ from native desktop APIs.

## 2. Interface Design (Abstract API)

The clipboard is implemented as a **static utility class** with no instance state. All operations are class-level.

### Type Definitions

```cpp
using GetTextFn = std::function<std::string()>;
using SetTextFn = std::function<void(const std::string&)>;
```

These function types define the signature for custom clipboard handlers.

### Public API

| Method | Signature | Description |
|--------|-----------|-------------|
| `setHandlers` | `static void setHandlers(GetTextFn, SetTextFn)` | Register custom clipboard handlers |
| `useDefaultHandlers` | `static void useDefaultHandlers()` | Reset to default GLFW clipboard |
| `getText` | `static std::string getText()` | Read text from clipboard |
| `setText` | `static void setText(const std::string&)` | Write text to clipboard |
| `hasCustomHandlers` | `static bool hasCustomHandlers()` | Check if using custom handlers |

### Private State

```cpp
static GetTextFn s_getText;      // Custom get text callback
static SetTextFn s_setText;      // Custom set text callback
static bool s_useCustom;         // Flag: using custom handlers?
```

## 3. Platform-Specific Implementations

### Default: GLFW Desktop Implementation

When no custom handlers are registered, the clipboard uses GLFW's native functions:

```cpp
// Reading from clipboard
GLFWwindow* window = glfwGetCurrentContext();
const char* text = glfwGetClipboardString(window);

// Writing to clipboard
glfwSetClipboardString(window, text.c_str());
```

**Key behaviors:**
- Requires a valid GLFW context (`glfwGetCurrentContext()`)
- Returns empty string if no window context exists
- Returns empty string if clipboard is empty or contains non-text data
- Null-safe: checks for null pointer from `glfwGetClipboardString`

### Custom Handler Implementation

For web or other platforms, custom handlers can be injected:

```cpp
// Example: Web platform registration
Clipboard::setHandlers(
    []() -> std::string {
        // JavaScript interop to read browser clipboard
        return getClipboardFromJS();
    },
    [](const std::string& text) {
        // JavaScript interop to write browser clipboard
        setClipboardInJS(text);
    }
);
```

The `s_useCustom` flag is set to `true` only when **both** handlers are provided.

## 4. How Clipboard Operations Work

### Copy Operation Flow

```
User presses Cmd/Ctrl+C
        |
        v
Engine::copySelection()
        |
        v
Extract selected text from inputBuffer
        |
        v
Clipboard::setText(selectedText)
        |
        +-- s_useCustom == true --> s_setText(text)
        |
        +-- s_useCustom == false --> glfwSetClipboardString()
```

### Paste Operation Flow

```
User presses Cmd/Ctrl+V
        |
        v
Engine::paste()
        |
        v
Clipboard::getText()
        |
        +-- s_useCustom == true --> return s_getText()
        |
        +-- s_useCustom == false --> return glfwGetClipboardString()
        |
        v
Insert text at cursor position
        |
        v
Update markdown content and re-render
```

## 5. Integration with the Engine

The Clipboard class is included in the engine header:

```cpp
// engine.h
#include "clipboard.h"
```

### Engine Methods Using Clipboard

#### `Engine::copySelection()`

Located at line 878 in `engine.cpp`:

```cpp
void Engine::copySelection() {
    if (!hasSelection) {
        return;
    }

    int start = std::min(selectionStart, selectionEnd);
    int end = std::max(selectionStart, selectionEnd);
    std::string selectedText(inputBuffer + start, end - start);
    Clipboard::setText(selectedText);
}
```

**Behavior:**
- No-op if no text is selected
- Handles reversed selections (selectionEnd < selectionStart)
- Extracts substring from raw input buffer

#### `Engine::paste()`

Located at line 889 in `engine.cpp`:

```cpp
void Engine::paste() {
    std::string clipboardText = Clipboard::getText();
    if (clipboardText.empty()) {
        return;
    }

    saveUndoState();

    // Delete selection if present
    if (hasSelection) {
        // ... delete selected text ...
    }

    // Insert clipboard text
    int pasteLen = clipboardText.length();
    if (inputLength + pasteLen < INPUT_BUFFER_SIZE - 1) {
        // ... insert text at cursor ...
    }
}
```

**Behavior:**
- No-op if clipboard is empty
- Saves undo state before modification
- Replaces selection if text is selected
- Buffer overflow protection (checks against INPUT_BUFFER_SIZE)
- Triggers markdown re-render after paste
- Calls `ensureCursorVisible()` to scroll to cursor

### Keyboard Shortcut Binding

Clipboard operations are triggered via platform-agnostic keyboard shortcuts:

```cpp
// Platform detection: CMD on macOS, CTRL on Windows/Linux
bool cmdOrCtrl = (mods & GLFW_MOD_SUPER) || (mods & GLFW_MOD_CONTROL);

if (cmdOrCtrl) {
    if (key == GLFW_KEY_C) {
        copySelection();
        return;
    } else if (key == GLFW_KEY_V) {
        paste();
        return;
    }
}
```

## 6. Notable Implementation Details

### No Cut Operation

The current implementation does **not** include a cut operation (Cmd/Ctrl+X). Users must copy, then delete separately.

### Static Class Design

The Clipboard class uses only static members and methods. This:
- Avoids needing to pass clipboard instances around
- Provides global access from anywhere in the codebase
- Simplifies the API for a singleton-like resource

### Thread Safety Considerations

The current implementation is **not thread-safe**. The static function pointers could be modified while in use. For single-threaded UI applications, this is typically acceptable.

### Error Handling

The implementation favors **graceful degradation**:
- Missing GLFW context: returns empty string, no crash
- Null clipboard content: returns empty string
- Empty paste content: early return, no modification

### Handler Registration Safety

The `s_useCustom` flag is only set to `true` when **both** handlers are non-null:

```cpp
s_useCustom = (getText != nullptr && setText != nullptr);
```

This prevents partial handler registration that could lead to asymmetric behavior.

### Memory Management

- `getText()` returns by value (`std::string`), so the caller owns the copy
- `setText()` takes a const reference, no ownership transfer
- No manual memory management required by users of the API

## File Locations

| File | Path |
|------|------|
| Header | `/home/liam/Documents/projects/promark/protos/a/src/engine/clipboard.h` |
| Implementation | `/home/liam/Documents/projects/promark/protos/a/src/engine/clipboard.cpp` |
| Engine Integration | `/home/liam/Documents/projects/promark/protos/a/src/engine/engine.cpp` |
