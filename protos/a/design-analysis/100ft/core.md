# Engine: Compressed Analysis

## Core Pattern: God Class with Dual Authority

The Engine owns everything: input handling, text editing, rendering coordination, undo, clipboard, URL opening, and UI chrome. More critically, it maintains **two parallel sources of truth**:

1. `inputBuffer` - a raw 10MB char array it mutates directly via `memmove`/`memcpy`
2. `textBuffer` - a TextBuffer object it owns but bypasses

Every mutation follows a dual-write pattern:
```cpp
memmove(inputBuffer + ..., ...);        // Direct mutation
textBuffer->setText(newText);            // Sync via full replacement
markdownRenderer->setTextBuffer(...);    // Forward copy
```

This appears 12+ times across editing, formatting, paste, and undo operations. Silent divergence is guaranteed if any path forgets both writes.

## Cross-Cutting: Position Space Mismatch

Engine thinks in **raw positions** (byte offsets in inputBuffer). MarkdownRenderer thinks in **DOM positions** (character offsets in parsed content). A translation layer bridges them:

```cpp
int domCursorPos = markdownRenderer->rawToDOM(cursorPos);  // Every frame
cursorPos = markdownRenderer->hitTest(...);                 // Every click
```

This indicates the architecture wanted cursor state in DOM-space, but direct buffer manipulation forced continuous translation.

## Cross-Cutting: Platform Embedding

GLFW is not abstracted - it's woven throughout:
- Key constants (`GLFW_KEY_*`, `GLFW_MOD_*`) in 260+ lines of input handling
- Timer (`glfwGetTime()`) in 8 call sites
- Mouse constants in click/drag handling

OpenGL calls appear directly in `initialize()` and `render()`. The Engine cannot be tested or ported without both libraries linked.

## Unstable Boundary: Raw Mode Parallel Implementation

`showRaw` mode (230 lines) reimplements text wrapping, hit testing, cursor positioning, selection rendering, and syntax highlighting - all concepts MarkdownRenderer handles. But with:
- Hardcoded font size vs layout system
- Monospace only vs proportional
- Different highlight colors

This is a shadow rendering pipeline with divergent behavior, not a view toggle.

## Paper Abstractions

| Abstraction | Declared | Actual |
|-------------|----------|--------|
| `TextBuffer` | Editing API with `insertText`/`deleteText` | Bypassed; used as serialization format via `setText()` |
| `UndoState` | Name implies undo/redo system | Only undo exists |
| `goalColumn` | Comment says "remembered for vertical nav" | Reset by horizontal movement, defeating purpose |
| Caret animation | Has target/current/lerp values | Ignored in raw mode; no time-based smoothing |

## Security Boundary Violation

```cpp
void Engine::openUrl(const std::string& url) {
    system(("open \"" + url + "\"").c_str());  // macOS only, shell injection possible
}
```

The rendering layer reaches out to OS shell with unsanitized input.

## Lazy Initialization Smell

`uiRendererInitialized` flag guards duplicate initialization in both `renderToolbar()` and `renderRawText()`. The UI subsystem was bolted on - same init code in two places, and it duplicates BatchRenderer/GlyphAtlas capabilities that MarkdownRenderer already provides internally.

## Summary

The Engine is an **abstraction-inverting god class** that owns high-level concepts (TextBuffer, MarkdownRenderer) but bypasses their APIs to do low-level work itself. The dual-write pattern, position translation layer, and raw mode parallel implementation are all workarounds for this fundamental inversion. Platform dependencies are embedded rather than injected, and security boundaries don't exist where they should.
