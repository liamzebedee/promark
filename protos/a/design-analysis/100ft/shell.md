# Shell Layer: Architectural Analysis

Synthesized from `main.cpp` and `edit.cpp` analyses. Both files serve as GLFW windowing shells that forward input to Engine.

---

## Repeated Patterns

### Global Engine Pointer + C Callback Accommodation
Both files use `Engine* engine = nullptr` as global state accessed by GLFW callbacks. This is a necessity-of-C-callbacks pattern that creates implicit coupling where any callback can mutate engine state without coordination.

### Coordinate Scaling at Shell Layer
Both implement HiDPI scaling by computing scale factors and transforming coordinates *before* forwarding to Engine:
```cpp
engine->handleMouse(button, action, mods, xpos * scaleX, ypos * scaleY);
```
Engine receives scaled coordinates but has no knowledge of the scaling factor. If Engine ever needs physical vs logical coordinates, this information is lost.

### Image Drag-Drop as Content Transformation
Both implement identical image-processing pipelines directly in the shell:
- `isImageFile()` - extension detection
- `getMimeType()` - MIME mapping
- `encodeFileToBase64()` - base64 algorithm
- Markdown syntax construction: `![alt](data:mime;base64,...)`

This is ~80 lines of content transformation logic duplicated across shells that belongs in Engine.

---

## Cross-Cutting Concerns

### Input API Asymmetry
Engine exposes clean input methods: `handleKeyboard()`, `handleScroll()`, `handleMouse()`, `handleMouseMove()`. But there is no `handleFileDrop()`. Both shells work around this by calling `insertText()` directly after doing their own content transformation.

**Evidence of workaround**: A consistent design would have:
```cpp
void dropCallback(...) {
    if (engine) engine->handleFileDrop(paths, count);
}
```

### Polling vs Event-Driven Architecture
Both shells poll Engine state every frame:
- `edit.cpp`: Calls `updateWindowTitle()` every frame, which copies entire buffer to string for dirty comparison
- `main.cpp`: Polls `engine->shouldClose()` to duplicate termination authority

Neither uses event callbacks from Engine to Shell.

### Lifecycle Signal Inversion
Pattern in both: Shell guesses when Engine is "ready" rather than Engine signaling readiness.
- Window show/hide based on frame count (workaround for initialization flicker)
- No "Engine ready for display" signal exists

---

## Unstable Boundaries

### Who Owns Dirty State?
**Engine declares**: `isDirty()`, `markClean()` methods exist on Engine.
**Shell implements**: `edit.cpp` ignores these entirely, implementing parallel dirty tracking by comparing `engine->getContent()` against cached `diskContent` string. Two competing dirty-tracking systems that can diverge.

### Who Generates Markdown Syntax?
**Engine owns**: Markdown parsing and rendering.
**Shell owns**: Markdown generation for images (`![alt](data:...)`).

Asymmetric. Domain knowledge (markdown syntax) has leaked into the shell layer.

### Who Owns Cursor State?
**Engine exposes**: `isOverLink()` for link hover detection.
**Shell implements**: Cursor icon switching between I-beam and hand.

The shell queries Engine for hover state but owns the cursor rendering. Tight coupling with no abstraction.

### Coordinate System Authority
Shell transforms coordinates before Engine receives them. Engine cannot implement its own coordinate systems or reason about physical pixels. The scaling factor is computed and discarded at shell level.

---

## Paper Abstractions

### Engine's Dirty/Close API
```cpp
bool isDirty() const;    // Never called by edit.cpp
void markClean();        // Never called by edit.cpp
bool shouldClose() const; // Never called by edit.cpp
```
These methods exist in Engine's interface but are completely bypassed. The shell implements parallel mechanisms.

### TextBuffer Position-Based API
Engine exposes `TextBuffer` with position-based operations:
```cpp
void insertText(size_t position, const std::string& text);
void deleteText(size_t position, size_t length);
```
Shell ignores this, using only `getContent()`/`setContent()` which copy entire buffer to string. Every dirty check performs full buffer copy.

### Clipboard Platform Abstraction
`clipboard.h` defines callback injection for platform portability. Initialization responsibility is unclear - shell never initializes it, presumably Engine does internally. The abstraction exists but ownership is invisible.

### File Drop Handler
Comment in `main.cpp` acknowledges: "always base64 since no save path". This admits a missing feature (file management) but implements the workaround (base64 embedding) at wrong layer. Future file-path-based images would require refactoring shell code that shouldn't exist there.

---

## Structural Summary

| Concern | Declared Location | Actual Location | Status |
|---------|-------------------|-----------------|--------|
| Dirty tracking | Engine | Shell | Duplicated, divergent |
| Markdown generation | Engine (parsing) | Shell (generation) | Split |
| Coordinate scaling | Platform abstraction | Shell | Hardcoded |
| File drop handling | Engine input API | Shell content transform | Bypassed |
| Cursor rendering | Shell | Shell queries Engine | Coupled |
| Lifecycle signaling | Engine | Shell guesses | Inverted |

The shell layer has accumulated domain logic (image processing, markdown syntax, dirty comparison) that violates the expected thin-shell pattern. Engine exposes APIs that are ignored while shells implement parallel mechanisms.
