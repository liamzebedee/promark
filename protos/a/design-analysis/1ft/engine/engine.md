# Engine Module Design Analysis

## 1. Responsibilities

The `Engine` class serves as the **application core**, acting as the central coordinator for a markdown editor. Its responsibilities span multiple domains:

### Text Document Management
- Owns the primary text buffer (`inputBuffer`, line 52-54) - a raw 10MB char array
- Maintains cursor position, selection state, and goal column for vertical navigation (lines 57-61)
- Provides content get/set API (lines 31-36, 988-1007)

### Input Event Processing
- Keyboard handling with platform-agnostic modifier detection (lines 226-490)
- Mouse handling including click detection (single/double/triple), drag selection, and link clicks (lines 498-613)
- Scroll handling (lines 492-496)

### Rendering Pipeline Coordination
- Initializes OpenGL context and FreeType font system (lines 41-121)
- Orchestrates markdown rendering via `MarkdownRenderer` (lines 155-181)
- Manages a parallel "raw mode" rendering path (lines 152-154, 1220-1454)
- Renders UI chrome: toolbar (lines 1456-1561) and scrollbar (lines 187-223)

### Text Editing Operations
- Character/word/line navigation (lines 639-877)
- Insert/delete operations with selection handling (lines 703-813)
- Clipboard integration (lines 886-931)
- Undo system (lines 973-1039)
- Markdown formatting helpers: bold, italic, headings, links (lines 1643-1792)

---

## 2. Dependencies

### Direct Header Dependencies (engine.h lines 2-10)

| Dependency | Purpose | Coupling Level |
|------------|---------|----------------|
| `ft2build.h`, `FT_FREETYPE_H` | Font loading and glyph metrics | Owns FT_Library + FT_Face |
| `markdown_renderer.h` | Markdown parse/layout/paint pipeline | Owns via unique_ptr |
| `clipboard.h` | System clipboard access | Static function calls |
| `batch_renderer.h` | OpenGL quad batching for UI | Owns via unique_ptr |
| `glyph_atlas.h` | Glyph texture cache | Owns via unique_ptr |

### Implicit Dependencies (engine.cpp lines 1-8)

| Dependency | Purpose | Concern |
|------------|---------|---------|
| `typography.h` | Layout constants (BASE_FONT_SIZE, DOCUMENT_MARGIN) | Used in raw mode only |
| `gl_includes.h` | OpenGL API | Direct GL calls in render() |
| `GLFW/glfw3.h` | Window timing (`glfwGetTime`), key constants | Deeply embedded |

### Why These Dependencies Exist

1. **FreeType**: Engine loads fonts and passes FT_Face to subsystems. This is intentional - font loading is a startup concern.

2. **MarkdownRenderer**: Encapsulates the parsing/layout/paint pipeline. Engine delegates rich-text rendering entirely.

3. **BatchRenderer + GlyphAtlas**: Used exclusively for toolbar and raw-mode rendering. Duplicates capability that MarkdownRenderer already provides internally.

4. **GLFW**: Timer and keyboard constants are pervasive. The Engine cannot be tested without GLFW being linked.

---

## 3. Mutation Points

### State Engine Owns and Mutates

| State | Location | Authority |
|-------|----------|-----------|
| `inputBuffer` / `inputLength` | lines 52-54 | Engine is authoritative |
| `cursorPos`, `selectionStart/End`, `hasSelection` | lines 57-61 | Engine is authoritative |
| `scrollOffset` | line 49 | Engine is authoritative |
| `undoStack` | line 101 | Engine is authoritative |
| `dirty` flag | line 44 | Engine is authoritative |
| `showRaw` toggle | line 132 | Engine is authoritative |

### State Engine Owns But Forwards

| State | Location | Forwarded To |
|-------|----------|--------------|
| `textBuffer` | line 65 | MarkdownRenderer (via setTextBuffer) |
| Font faces (face, monoFace) | lines 69-70 | MarkdownRenderer (via setFontFace) |
| CaretState | lines 160-169 | MarkdownRenderer (via setCaretState) |

### Problematic Dual-Write Pattern

Every text mutation follows this pattern:

```cpp
// Direct buffer mutation
memmove(inputBuffer + ..., ...);
inputLength += ...;

// Sync to MarkdownRenderer
std::string newText(inputBuffer, inputLength);
textBuffer->setText(newText);
markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
```

This pattern appears at:
- lines 366-370, 389-393, 413-417, 425-429 (delete operations)
- lines 722-727 (insertChar)
- lines 760-764 (insertText)
- lines 784-788 (deleteChar)
- lines 805-809 (deleteWordBackward)
- lines 924-928 (paste)
- lines 999-1002 (setContent)
- lines 1031-1034 (undo)
- lines 1636-1640 (wrapSelection)
- lines 1655-1659, 1676-1680, 1723-1727, 1765-1769, 1785-1789 (formatting operations)

**Problem**: The `inputBuffer` and `textBuffer` are never guaranteed to be in sync. If any code path forgets to update both, they diverge silently.

---

## 4. Boundary Violations

### GLFW Leakage into Core Logic

The Engine directly references GLFW constants and functions throughout:

- **Key constants** (GLFW_KEY_*, GLFW_MOD_*): lines 227-488
- **Mouse constants** (GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, GLFW_RELEASE): lines 501-586
- **Timer** (`glfwGetTime()`): lines 146, 293, 511, 669, 731, 769, 874, 1037

This couples the Engine to GLFW at compile time. The Engine cannot be unit-tested or ported to a different windowing system without significant modification.

### Direct OpenGL Calls

Engine calls OpenGL directly in `initialize()` and `render()`:

- lines 46-50: `glEnable`, `glBlendFunc`
- lines 113-116: `glGetError`
- lines 134-136: `glViewport`, `glClearColor`, `glClear`
- lines 142-143: `glEnable(GL_SCISSOR_TEST)`, `glScissor`
- line 184: `glDisable(GL_SCISSOR_TEST)`

The Engine is acting as both the scene graph coordinator AND the GL context manager. These should be separate concerns.

### Shell Command Execution (Security Boundary)

Line 592-594:
```cpp
void Engine::openUrl(const std::string& url) {
    std::string command = "open \"" + url + "\"";
    system(command.c_str());
}
```

This is:
1. Platform-specific (macOS only)
2. A potential command injection vector if URL contains shell metacharacters
3. A violation of the rendering layer reaching out to the OS shell

---

## 5. Declared-but-Unrealised Design

### TextBuffer Abstraction (Bypassed)

**Declaration** (engine.h line 65):
```cpp
std::unique_ptr<TextBuffer> textBuffer;
```

**Reality**: Engine owns a TextBuffer but never uses its `insertText()` or `deleteText()` methods. Instead, Engine:
1. Mutates its own `inputBuffer` with raw `memmove`/`memcpy` calls
2. Creates a new string from `inputBuffer`
3. Calls `textBuffer->setText()` with the complete string
4. Creates a new `TextBuffer` copy and passes it to MarkdownRenderer

The TextBuffer's editing API (insertText, deleteText) is completely unrealised. The architecture implies TextBuffer should be the source of truth, but Engine treats it as a serialization format.

### Dual Rendering Paths (Asymmetric Implementation)

**showRaw mode** (lines 1220-1454) reimplements:
- Text wrapping logic
- Hit testing
- Cursor positioning
- Selection rendering
- Syntax highlighting

This ~230-line parallel implementation duplicates concepts that MarkdownRenderer handles, but with different behavior:
- Uses `Typography::BASE_FONT_SIZE` hardcoded vs MarkdownRenderer's layout system
- Uses monospace font exclusively vs proportional
- Implements its own syntax highlighting colors vs MarkdownRenderer's styled spans

### rawToDOM/domToRaw Mapping (Workaround Code)

Lines 156-158:
```cpp
int domCursorPos = markdownRenderer->rawToDOM(cursorPos);
int domSelStart = markdownRenderer->rawToDOM(selectionStart);
int domSelEnd = markdownRenderer->rawToDOM(selectionEnd);
```

And line 539:
```cpp
cursorPos = markdownRenderer->hitTest(...);  // Returns RAW position
```

The mapping system exists because:
1. Engine thinks in "raw" positions (byte offsets in inputBuffer)
2. MarkdownRenderer thinks in "DOM" positions (character offsets in parsed content)

This implies the architecture planned for cursor positions to live in DOM-space, but Engine's direct buffer manipulation forced a translation layer. The translation is called on every render frame and every mouse click.

### UndoState Without Redo (Incomplete)

**Declaration** (engine.h lines 12-15, 101-104):
```cpp
struct UndoState {
    std::string text;
    int cursorPos;
};
std::vector<UndoState> undoStack;
```

Only `undo()` exists; there is no redo functionality. The naming suggests a complete undo/redo system was intended but never finished.

### goalColumn (Partially Realised)

**Declaration** (engine.h line 58):
```cpp
int goalColumn;  // Remembered column for vertical navigation
```

**Implementation**: Updated in many places but only actually used in `moveCursorVertically()` (line 850). The comment implies it should persist across horizontal movements, but `moveCursor()` (line 666) and `moveCursorByWord()` (line 687) both reset it, defeating the purpose.

### Caret Animation (Incomplete Integration)

**Declaration** (engine.h lines 123-129):
```cpp
float caretAnimX, caretAnimY;
float caretTargetX, caretTargetY;
```

**Reality**: Animation values are computed (lines 956-971) and passed to MarkdownRenderer via CaretState, but:
- In raw mode, animation is completely ignored (cursor snaps)
- The animation uses a fixed lerp factor of 0.4f (line 964) with no time-based smoothing
- Snap-on-click (lines 576-578) creates visual discontinuity

### UI Renderer Lazy Initialization (Code Smell)

The `uiRendererInitialized` flag (lines 115, 1222, 1458) guards lazy initialization of `uiRenderer` and `uiAtlas`. This happens in:
- `renderToolbar()`
- `renderRawText()`

This pattern suggests the UI rendering subsystem was bolted on after the initial design. It duplicates the same initialization code in two places.

---

## Summary of Architectural Concerns

1. **God Class**: Engine handles input, rendering, text editing, undo, clipboard, URL opening, and UI rendering. These should be separate concerns.

2. **Dual Source of Truth**: `inputBuffer` and `textBuffer` must be kept in sync manually. This is error-prone.

3. **Platform Lock-in**: GLFW types and functions are embedded throughout, preventing testing and portability.

4. **Parallel Implementations**: Raw mode duplicates layout/rendering logic with different behavior.

5. **Abstraction Inversion**: Engine owns TextBuffer but bypasses its API, doing manual buffer manipulation instead.

6. **Security**: `system()` call for URL opening is both platform-specific and potentially unsafe.
