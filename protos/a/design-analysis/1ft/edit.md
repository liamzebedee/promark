# Design Analysis: src/edit.cpp

## Overview

`edit.cpp` is the application entry point and main event loop for the markdown editor. It functions as the host shell, bridging GLFW windowing with the Engine subsystem.

---

## 1. Responsibilities

This file must:

1. **Parse CLI arguments** (lines 253-257) - Extract file path from command line
2. **Initialize GLFW windowing** (lines 265-279) - Create window, set OpenGL context hints
3. **Register input callbacks** (lines 286-290) - Wire keyboard, scroll, mouse, cursor, and drop events
4. **Manage file I/O** (lines 33-61) - Load file on startup, save on Cmd+S
5. **Track dirty state** (lines 13-31) - Compare buffer against disk content, update window title
6. **Handle image drag-and-drop** (lines 63-191) - Convert dropped images to markdown syntax
7. **Coordinate cursor rendering** (lines 232-251) - Switch between I-beam and hand cursor based on link hover
8. **Run main loop** (lines 312-328) - Poll events, render, swap buffers

---

## 2. Dependencies

| Dependency | Location | Purpose |
|------------|----------|---------|
| `GLFW/glfw3.h` | line 1 | Windowing, input, cursor management |
| `engine/engine.h` | line 7 | All editor functionality |
| `<fstream>`, `<sstream>` | lines 3-4 | File loading/saving |
| `<vector>`, `<cctype>` | lines 5-6 | Base64 encoding, string utilities |

### Dependency Analysis

**GLFW** is used directly rather than through an abstraction. This couples the shell to a specific windowing library. Lines 214-220 (`getDisplayScale`) and lines 222-251 (coordinate scaling) implement HiDPI handling that arguably belongs in a platform abstraction.

**Engine** is a monolithic interface (136 lines in header). edit.cpp treats it as a black box, calling:
- `initialize()`, `render()` (lifecycle)
- `handleKeyboard()`, `handleScroll()`, `handleMouse()`, `handleMouseMove()` (input dispatch)
- `setContent()`, `getContent()`, `insertText()` (document mutation)
- `isOverLink()` (cursor state query)

---

## 3. Mutation Points

### Global State (lines 9-14)
```cpp
Engine* engine = nullptr;
GLFWwindow* window = nullptr;
std::string filePath;
std::string fileName;
std::string diskContent;
bool lastDirtyState = false;
```

**Concern**: Six globals with implicit ordering dependencies. `engine` must be initialized before any callback fires. `diskContent` shadows engine state for dirty detection.

### Document Content
| Location | Mutator | Authority Issue |
|----------|---------|-----------------|
| line 307 | `engine->setContent(content)` | Shell loads, engine owns |
| line 47 | `diskContent = content` | Shell caches disk state separately |
| line 187 | `engine->insertText(markdown)` | Shell generates markdown syntax |

**Concern**: Dirty state authority is split. Engine has `isDirty()` (line 34 in engine.h) and `markClean()`, but edit.cpp ignores these and implements its own `isDirty()` (line 16) by comparing `engine->getContent()` against `diskContent`. Two competing dirty-tracking systems exist.

### Window Title (lines 21-31)
The shell owns window title state (`lastDirtyState`) and updates it every frame (line 319). This polls engine content every frame just to detect changes.

---

## 4. Boundary Violations

### Layer Inversion: Shell Implements Domain Logic

**Image markdown generation** (lines 140-177):
```cpp
std::string buildImageMarkdown(const std::string& imagePath) {
    // ... 40 lines of markdown syntax construction
    return "\n![" + altText + "](data:" + mimeType + ";base64," + base64 + ")\n";
}
```
The shell constructs markdown syntax, which is domain knowledge. Engine owns markdown parsing/rendering but shell owns markdown generation. Asymmetric.

**Base64 encoding** (lines 86-111):
A 25-line base64 implementation in the application shell. This is utility code that belongs in a shared module.

**Path manipulation** (lines 113-138):
```cpp
std::string getDirectoryPath(const std::string& filePath);
std::string computeRelativePath(const std::string& fromFile, const std::string& toFile);
```
File path utilities implemented in the shell. No equivalent abstraction exists in engine.

### Layer Inversion: Shell Reaches Into Engine Details

**Coordinate scaling** (lines 214-220, 227-228, 239-241):
```cpp
float scaleX, scaleY;
getDisplayScale(window, scaleX, scaleY);
engine->handleMouse(button, action, mods, xpos * scaleX, ypos * scaleY);
```
Shell transforms coordinates before passing to engine. Engine receives scaled coordinates but has no visibility into the scaling factor. This prevents engine from implementing its own coordinate systems.

---

## 5. Declared-but-Unrealised Design

### Engine's Dirty/Close API (engine.h lines 34-36)

```cpp
bool isDirty() const { return dirty; }
void markClean() { dirty = false; }
bool shouldClose() const { return wantsToClose; }
```

**Unrealised**: edit.cpp never calls `engine->isDirty()` or `engine->markClean()`. Instead it implements parallel dirty tracking (line 16-19). The engine's dirty flag and shell's `diskContent` comparison can diverge.

**Unrealised**: `shouldClose()` is never called. The shell relies on `glfwWindowShouldClose()` (line 312) instead of asking the engine.

### TextBuffer Abstraction (text_buffer.h)

Engine exposes `std::unique_ptr<TextBuffer> textBuffer` (engine.h line 65), a proper buffer abstraction with:
```cpp
void insertText(size_t position, const std::string& text);
void deleteText(size_t position, size_t length);
```

**Bypassed**: edit.cpp accesses content only through `getContent()`/`setContent()` string copies (lines 18, 36, 307). The buffer's position-based API is invisible to the shell. Every dirty check copies the entire buffer to a string.

### CaretState Abstraction (markdown_renderer.h lines 12-21)

```cpp
struct CaretState {
    int cursorPosition = 0;
    int selectionStart = 0;
    // ... animation fields
};
```

A proper caret state object exists, but the shell has no access to it. Shell implements its own cursor animation state via globals (`currentlyOverLink`, line 234) for cursor icon switching.

### Clipboard Platform Abstraction (clipboard.h)

```cpp
static void setHandlers(GetTextFn getText, SetTextFn setText);
static void useDefaultHandlers();
```

A platform-agnostic clipboard exists with callback injection for web. But edit.cpp never initializes it - presumably engine does this internally. The abstraction exists but initialization responsibility is unclear.

---

## Summary of Architectural Concerns

1. **Dual dirty tracking**: Engine has dirty flag, shell compares strings. Pick one.

2. **Domain logic leakage**: Markdown generation, base64 encoding, path resolution live in shell.

3. **Scaling responsibility unclear**: Shell scales coordinates before engine sees them. Engine cannot reason about physical vs logical coordinates.

4. **Polling architecture**: `updateWindowTitle()` called every frame polls content for dirty check. Should be event-driven.

5. **Global state coupling**: Six globals require careful initialization order. No lifecycle object.

6. **Unused engine APIs**: `shouldClose()`, `isDirty()`, `markClean()` defined but ignored.
