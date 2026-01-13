# Main Entry Point and Edit Functionality Specification

## 1. Purpose and Overview

The promark markdown editor has two entry point implementations:

| File | Purpose | Use Case |
|------|---------|----------|
| `main.cpp` | Demo/development entry point | Quick testing with hardcoded sample content |
| `edit.cpp` | Production entry point | Full-featured file editing with save/load |

Both files serve as the application's main entry point, initializing GLFW for windowing, setting up input callbacks, and managing the render loop. They delegate all text editing, parsing, and rendering to the `Engine` class.

### Key Differences

| Feature | main.cpp | edit.cpp |
|---------|----------|----------|
| File argument | None (demo content) | Required (`mdedit <file.md>`) |
| Save functionality | Not supported | Cmd/Ctrl+S saves to file |
| Dirty tracking | Via Engine only | Tracks disk vs. editor content |
| Window title | Static "MD Editor" | Dynamic filename + "[unsaved]" indicator |
| Link hover cursor | I-beam only | I-beam and hand cursor switching |
| Close handling | Engine-controlled | Standard window close |

## 2. Key Functions and Their Responsibilities

### Shared Functions (Both Files)

#### `keyCallback(GLFWwindow*, int key, int scancode, int action, int mods)`
Handles keyboard input. Forwards all key events to `Engine::handleKeyboard()`.

**edit.cpp enhancement**: Intercepts Cmd/Ctrl+S before forwarding to engine for file save operations.

#### `scrollCallback(GLFWwindow*, double xoffset, double yoffset)`
Handles mouse wheel/trackpad scroll. Forwards to `Engine::handleScroll()`.

#### `mouseButtonCallback(GLFWwindow*, int button, int action, int mods)`
Handles mouse button events. Converts window coordinates to framebuffer coordinates (for Retina/HiDPI support) and forwards to `Engine::handleMouse()`.

#### `cursorPosCallback(GLFWwindow*, double xpos, double ypos)`
Handles mouse movement. Converts coordinates and forwards to `Engine::handleMouseMove()`.

**edit.cpp enhancement**: Additionally checks `Engine::isOverLink()` and switches cursor between I-beam and hand.

#### `dropCallback(GLFWwindow*, int count, const char** paths)`
Handles drag-and-drop of files onto the window. Filters for image files and inserts markdown image syntax.

#### `getDisplayScale(GLFWwindow*, float& scaleX, float& scaleY)`
Calculates the ratio between window size and framebuffer size for HiDPI/Retina display support.

#### `isImageFile(const std::string& path) -> bool`
Checks if a file path has an image extension (png, jpg, jpeg, gif, webp, bmp).

#### `getMimeType(const std::string& path) -> std::string`
Returns the MIME type string for an image file based on its extension.

#### `encodeFileToBase64(const std::string& path) -> std::string`
Reads a binary file and encodes it to a base64 string for data URI embedding.

### edit.cpp-Specific Functions

#### `loadFile(const std::string& path) -> std::string`
Reads entire file content into a string using stream buffers.

#### `saveFile() -> bool`
Writes current editor content to the file path. Updates `diskContent` reference and refreshes window title.

#### `isDirty() -> bool`
Compares current editor content with `diskContent` to determine if changes exist.

#### `updateWindowTitle()`
Sets window title to filename, appending " [unsaved]" if dirty. Only updates GLFW when state changes.

#### `getDirectoryPath(const std::string& filePath) -> std::string`
Extracts the directory portion from a file path.

#### `computeRelativePath(const std::string& fromFile, const std::string& toFile) -> std::string`
Computes a relative path from the document to an image file, enabling portable image references.

#### `buildImageMarkdown(const std::string& imagePath) -> std::string`
Constructs markdown image syntax. Uses relative paths when possible (document is saved), falls back to base64 data URIs otherwise.

### main() Function

**main.cpp version**:
1. Initialize GLFW
2. Create 800x600 window titled "MD Editor"
3. Set up input callbacks
4. Initialize Engine
5. Set hardcoded demo content (lists demo)
6. Run render loop until `Engine::shouldClose()` or window close
7. Cleanup

**edit.cpp version**:
1. Validate command-line argument (require file path)
2. Extract filename from path
3. Initialize GLFW
4. Create 800x600 window titled with filename
5. Set up input callbacks (including hand cursor for links)
6. Initialize Engine
7. Load file content and set on Engine
8. Store `diskContent` for dirty tracking
9. Run render loop with title updates each frame
10. Cleanup

## 3. Data Structures Used

### Global State Variables

| Variable | Type | File | Purpose |
|----------|------|------|---------|
| `engine` | `Engine*` | Both | Pointer to the main editor engine |
| `window` | `GLFWwindow*` | edit.cpp | GLFW window handle (needed for title updates) |
| `filePath` | `std::string` | edit.cpp | Full path to the open file |
| `fileName` | `std::string` | edit.cpp | Filename portion for display |
| `diskContent` | `std::string` | edit.cpp | Content as last saved/loaded from disk |
| `lastDirtyState` | `bool` | edit.cpp | Cache to avoid redundant title updates |
| `ibeamCursor` | `GLFWcursor*` | Both | I-beam cursor for text editing |
| `handCursor` | `GLFWcursor*` | edit.cpp | Hand cursor for hovering over links |
| `currentlyOverLink` | `bool` | edit.cpp | Tracks cursor state to avoid redundant updates |

### Engine Class (from engine.h)

The Engine class manages all editor state:

```cpp
class Engine {
    // Core editing state
    char* inputBuffer;           // 10MB text buffer
    int inputLength;             // Current text length
    int cursorPos;               // Caret position
    int selectionStart/End;      // Selection range
    bool hasSelection;

    // Navigation
    int goalColumn;              // Remembered column for vertical movement
    float scrollOffset;          // Vertical scroll position

    // Rendering
    std::unique_ptr<MarkdownRenderer> markdownRenderer;
    std::unique_ptr<BatchRenderer> uiRenderer;

    // Undo
    std::vector<UndoState> undoStack;
};
```

### UndoState Structure

```cpp
struct UndoState {
    std::string text;    // Complete document text
    int cursorPos;       // Cursor position at time of snapshot
};
```

## 4. Control Flow

### Application Startup

```
main()
  |
  +-- [edit.cpp only] Parse argv, extract filename
  |
  +-- glfwInit()
  |
  +-- glfwCreateWindow(800, 600, ...)
  |
  +-- glfwMakeContextCurrent()
  |
  +-- glfwHideWindow()              // Hide until first frame ready
  |
  +-- Set GLFW callbacks:
  |     - keyCallback
  |     - scrollCallback
  |     - mouseButtonCallback
  |     - cursorPosCallback
  |     - dropCallback
  |
  +-- Create cursors (I-beam, [hand in edit.cpp])
  |
  +-- engine = new Engine()
  |
  +-- engine->initialize()
  |     |
  |     +-- initFreeType()
  |     +-- loadFont()
  |     +-- Create MarkdownRenderer
  |     +-- Create BatchRenderer
  |
  +-- [main.cpp] engine->setContent(hardcoded demo)
  +-- [edit.cpp] loadFile() -> engine->setContent()
  |
  +-- Render loop (see below)
  |
  +-- Cleanup (delete engine, destroy cursors, glfwTerminate)
```

### Main Render Loop

```
while (!glfwWindowShouldClose(window))
  |
  +-- glfwPollEvents()              // Process input
  |
  +-- [main.cpp] Check engine->shouldClose()
  |
  +-- glfwGetFramebufferSize()      // Get current dimensions
  |
  +-- engine->render(width, height)
  |     |
  |     +-- Clear viewport
  |     +-- Render markdown content
  |     +-- Render toolbar
  |     +-- Render caret
  |     +-- Render selection highlights
  |
  +-- [edit.cpp] updateWindowTitle()
  |
  +-- glfwSwapBuffers()             // Present frame
  |
  +-- [First frame only] glfwShowWindow()
```

### Input Processing Flow

```
GLFW Event
  |
  +-- Callback invoked (key/mouse/scroll)
        |
        +-- [edit.cpp] Check for Cmd+S -> saveFile()
        |
        +-- Scale coordinates for HiDPI if needed
        |
        +-- Forward to Engine method
              |
              +-- Engine processes event
              +-- Updates internal state
              +-- May trigger re-layout
```

### Image Drop Flow

```
dropCallback()
  |
  +-- For each dropped path:
        |
        +-- isImageFile() check
        |
        +-- [main.cpp] Always use base64:
        |     +-- getMimeType()
        |     +-- encodeFileToBase64()
        |     +-- Build data URI markdown
        |
        +-- [edit.cpp] Try relative path first:
              |
              +-- If filePath exists:
              |     +-- computeRelativePath()
              |     +-- If relative works: use file reference
              |     +-- Else: fall back to base64
              |
              +-- If no filePath: use base64
              |
              +-- engine->insertText(markdown)
```

## 5. Dependencies on Other Components

### External Libraries

| Library | Header | Purpose |
|---------|--------|---------|
| GLFW | `<GLFW/glfw3.h>` | Window creation, input handling, OpenGL context |

### Internal Components

| Component | Header | Relationship |
|-----------|--------|--------------|
| Engine | `engine/engine.h` | Core editor logic, receives all input |
| MarkdownRenderer | `markdown_renderer.h` | Parses and renders markdown (via Engine) |
| Clipboard | `clipboard.h` | Copy/paste operations (via Engine) |
| BatchRenderer | `batch_renderer.h` | OpenGL rendering primitives (via Engine) |
| GlyphAtlas | `glyph_atlas.h` | Font texture management (via Engine) |

### Standard Library

- `<iostream>` - Error output
- `<fstream>` - File I/O (loading, saving, base64 encoding)
- `<sstream>` - String stream for file loading (edit.cpp)
- `<vector>` - Byte array for base64 encoding
- `<cctype>` - `std::tolower` for extension normalization

## 6. Notable Implementation Details

### HiDPI/Retina Display Support

Both files handle high-DPI displays by calculating a scale factor:

```cpp
void getDisplayScale(GLFWwindow* window, float& scaleX, float& scaleY) {
    int winW, winH, fbW, fbH;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);
    scaleX = (winW > 0) ? (float)fbW / winW : 1.0f;
    scaleY = (winH > 0) ? (float)fbH / winH : 1.0f;
}
```

This ratio converts mouse positions from window coordinates to framebuffer coordinates.

### Deferred Window Display

Both implementations hide the window initially and only show it after the first frame renders:

```cpp
glfwHideWindow(window);
// ... initialization ...
bool windowShown = false;
while (!glfwWindowShouldClose(window)) {
    // ... render ...
    if (!windowShown) {
        glfwShowWindow(window);
        windowShown = true;
    }
}
```

This prevents a flash of unrendered content on startup.

### Cross-Platform Modifier Key Handling

Keyboard shortcuts check both Control and Super (Command) modifiers:

```cpp
bool cmdOrCtrl = (mods & GLFW_MOD_SUPER) || (mods & GLFW_MOD_CONTROL);
```

This allows Cmd+S on macOS and Ctrl+S on Linux/Windows to both work.

### Dirty State Optimization

edit.cpp tracks dirty state changes to avoid redundant GLFW calls:

```cpp
void updateWindowTitle() {
    bool currentDirty = isDirty();
    if (currentDirty != lastDirtyState) {
        // Only update title when state changes
        glfwSetWindowTitle(window, title.c_str());
        lastDirtyState = currentDirty;
    }
}
```

### Image Embedding Strategy

edit.cpp uses a smart fallback for image embedding:

1. **Preferred**: Relative file paths (when document is saved and image is accessible)
2. **Fallback**: Base64 data URIs (for unsaved documents or unreachable images)

main.cpp always uses base64 since there is no file path context.

### Base64 Encoding Implementation

Manual base64 encoding without external dependencies:

```cpp
std::string encodeFileToBase64(const std::string& path) {
    // Read binary file
    std::vector<uint8_t> data(...);

    // Standard base64 alphabet
    static const char* base64Chars = "ABCDEF...";

    // Process 3 bytes at a time -> 4 base64 chars
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t n = data[i] << 16;
        if (i + 1 < data.size()) n |= data[i + 1] << 8;
        if (i + 2 < data.size()) n |= data[i + 2];

        result += base64Chars[(n >> 18) & 0x3F];
        result += base64Chars[(n >> 12) & 0x3F];
        // ... with '=' padding for incomplete groups
    }
}
```

### OpenGL Version

Both files configure OpenGL 2.1 with the "any profile" setting for maximum compatibility:

```cpp
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
```

### Cursor Management

edit.cpp maintains two cursors and switches between them based on content:

```cpp
GLFWcursor* ibeamCursor = nullptr;  // Text editing
GLFWcursor* handCursor = nullptr;   // Link hover

void cursorPosCallback(...) {
    bool overLink = engine->isOverLink(scaledX, scaledY);
    if (overLink != currentlyOverLink) {
        currentlyOverLink = overLink;
        glfwSetCursor(win, overLink ? handCursor : ibeamCursor);
    }
}
```

This provides visual feedback when hovering over clickable links.

### Window Close Behavior

main.cpp respects an engine-controlled close signal:

```cpp
if (engine->shouldClose()) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    break;
}
```

edit.cpp relies solely on the standard GLFW window close mechanism.
