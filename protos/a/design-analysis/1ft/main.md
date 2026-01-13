# Design Analysis: src/main.cpp

## 1. Responsibilities

This file serves as the application entry point and windowing shell. Its responsibilities are:

1. **GLFW Lifecycle Management** (L120-188): Initialize/terminate GLFW, create window, run event loop
2. **Input Routing** (L81-118, L142-146): Register callbacks for keyboard, mouse, scroll, cursor, and drag-drop events; forward to Engine
3. **Display Scaling** (L94-100): Compute Retina/HiDPI scale factors for coordinate transformation
4. **Image Drag-and-Drop** (L11-79): Detect image files, encode to base64, and insert as markdown

## 2. Dependencies

| Dependency | Usage | Justification |
|------------|-------|---------------|
| `GLFW/glfw3.h` (L1) | Window creation, event handling, OpenGL context | Platform abstraction for windowing |
| `iostream` (L2) | Error logging to stderr | Diagnostics |
| `fstream` (L3) | Reading dropped image files | Drag-drop support |
| `vector` (L4) | Temporary buffer for file contents | Base64 encoding |
| `cctype` (L5) | `tolower` for extension comparison | File type detection |
| `engine/engine.h` (L6) | Application logic delegation | Core editor functionality |

## 3. Mutation Points

| What | Where | Current Authority | Should Be |
|------|-------|-------------------|-----------|
| Global `engine` pointer | L8 | main.cpp | **Problematic** - global mutable state accessed by callbacks |
| Window visibility | L140, L179-182 | main.cpp | Correct - shell concern |
| Engine content | L160 | main.cpp | **Problematic** - hardcoded demo content should be config/argument |
| Cursor style | L149-150 | main.cpp | **Questionable** - cursor state is presentation that could vary by editor mode |

### Authority Concerns

The global `Engine* engine` (L8) is a classic necessity-of-C-callbacks pattern, but it creates implicit coupling where any callback can mutate engine state without coordination. The callbacks at L81-118 are pure pass-through, which is correct, but the `dropCallback` (L59-79) performs significant content transformation before calling `engine->insertText()`.

## 4. Boundary Violations

### 4.1 Image Processing Logic (L11-79)

**Severity: Medium-High**

Functions `isImageFile()`, `getMimeType()`, and `encodeFileToBase64()` implement a complete image-to-markdown pipeline directly in main.cpp. This is content transformation logic that belongs in the Engine or a dedicated media handler.

**Evidence:**
- L11-17: File extension parsing and image type detection
- L19-30: MIME type mapping
- L32-57: Base64 encoding algorithm
- L74: Markdown syntax construction (`![alt](data:mime;base64,...)`)

This violates the expected layering where main.cpp should only:
1. Detect "something was dropped"
2. Forward raw paths to Engine

The Engine should decide how to handle different file types, not the windowing shell.

### 4.2 Coordinate Scaling (L94-100, L106-108, L114-116)

**Severity: Low**

Display scale computation is performed in main.cpp and applied before forwarding to Engine. This means Engine receives scaled coordinates but has no knowledge of the scaling factor. If Engine ever needs to reason about physical vs logical coordinates, this information is lost.

## 5. Declared-but-Unrealised Design

### 5.1 Comment at L10: "Image drag-and-drop support (always base64 since no save path)"

**Declared:** Images are embedded as base64 because there's no save path mechanism.
**Unrealised:** This comment acknowledges a missing feature (file management/save paths) but the workaround (base64 embedding) is implemented at the wrong layer. It also implies future support for file-path-based images that would require refactoring this logic anyway.

### 5.2 Engine Interface Asymmetry

**Declared:** Engine has a clean API with `handleKeyboard()`, `handleScroll()`, `handleMouse()`, `handleMouseMove()` (L82-116).
**Unrealised:** There is no `handleDrop()` or `handleFileDrop()` method. Instead, main.cpp calls `insertText()` directly (L75), bypassing the input-handling abstraction. This asymmetry suggests the drag-drop feature was added without extending the Engine interface.

**Workaround Code:** The entire block at L59-79 is workaround code. A consistent design would have:
```cpp
void dropCallback(GLFWwindow* win, int count, const char** paths) {
    if (engine) engine->handleFileDrop(paths, count);
}
```

### 5.3 Engine Lifecycle Ceremony

**Declared:** L152-158 shows careful initialization with error handling.
**Unrealised:** L160 immediately violates this by hardcoding demo content. A properly initialized Engine should either:
1. Start empty
2. Accept content via constructor/initialize()
3. Load from a file path argument

The `setContent()` call with hardcoded markdown is prototyping residue that should be removed or externalized.

### 5.4 Window Show/Hide Pattern (L139-140, L162, L179-182)

**Declared:** Comment at L139 "Hide window until first frame is rendered" suggests intentional UX polish.
**Unrealised:** The `windowShown` flag and delayed show pattern is a workaround for visual flicker during initialization. A cleaner design would have Engine signal "ready for display" rather than main.cpp guessing after one frame.

### 5.5 Close Request Polling (L166-170)

**Declared:** Engine exposes `shouldClose()` for signaling close intent.
**Unrealised:** This is polled every frame in the main loop rather than being event-driven. The pattern `if (engine->shouldClose()) glfwSetWindowShouldClose(...)` duplicates termination authority between Engine and GLFW. Engine could directly call a shell callback or main.cpp could register for a close-request event.

## Summary

main.cpp correctly fulfills its role as a thin windowing shell for most input types, but has accumulated image-processing logic (L11-79) that belongs in the Engine layer. The global Engine pointer is a necessary C-callback accommodation but should be encapsulated. Hardcoded demo content (L160) and the absence of a `handleFileDrop()` method reveal prototyping artifacts that should be cleaned up for a production architecture.
