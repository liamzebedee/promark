# Engine Component Specification

## 1. Purpose and Overview

The `Engine` class is the central orchestrator of the promark markdown editor. It serves as the main controller that coordinates:

- **Rendering**: Manages the OpenGL rendering pipeline, including both rendered markdown view and raw text mode
- **Input Processing**: Handles all keyboard and mouse input events
- **Text Editing**: Maintains the document buffer and implements text manipulation operations
- **State Management**: Tracks cursor position, selection, scroll offset, and document dirty state
- **Component Integration**: Coordinates the markdown parser, layout engine, and rasterizer

The engine operates in two modes:
1. **Rendered Mode** (default): Displays formatted markdown using the `MarkdownRenderer`
2. **Raw Mode**: Shows the raw markdown source with syntax highlighting

## 2. Engine Class Structure and Member Variables

### 2.1 Core State Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `wantsToClose` | `bool` | Flag indicating user requested window close (Cmd/Ctrl+W) |
| `leftMouseHeld` | `bool` | Tracks if left mouse button is currently pressed |
| `dirty` | `bool` | Indicates unsaved changes to the document |
| `scrollOffset` | `float` | Current vertical scroll position in pixels |
| `contentHeight` | `float` | Total height of document content |
| `viewportHeight` | `int` | Height of visible content area (excludes toolbar) |
| `viewportWidth` | `int` | Width of viewport |
| `showRaw` | `bool` | Toggle between rendered and raw text mode |

### 2.2 Text Buffer

| Variable | Type | Purpose |
|----------|------|---------|
| `inputBuffer` | `char*` | Raw character buffer for document content |
| `INPUT_BUFFER_SIZE` | `const int` | Maximum buffer size (10MB) |
| `inputLength` | `int` | Current length of content in buffer |

### 2.3 Cursor and Selection State

| Variable | Type | Purpose |
|----------|------|---------|
| `cursorPos` | `int` | Current cursor position (character offset) |
| `goalColumn` | `int` | Remembered column for vertical navigation |
| `selectionStart` | `int` | Start of selection range |
| `selectionEnd` | `int` | End of selection range |
| `hasSelection` | `bool` | Whether a selection is active |

### 2.4 Caret Animation

| Variable | Type | Purpose |
|----------|------|---------|
| `caretAnimX` | `float` | Current animated X position of caret |
| `caretAnimY` | `float` | Current animated Y position of caret |
| `caretTargetX` | `float` | Target X position for caret animation |
| `caretTargetY` | `float` | Target Y position for caret animation |
| `lastBlinkTime` | `double` | Timestamp of last blink toggle |
| `caretVisible` | `bool` | Current visibility state of caret |

### 2.5 Click Detection

| Variable | Type | Purpose |
|----------|------|---------|
| `lastClickTime` | `double` | Timestamp of last mouse click |
| `lastClickX` | `double` | X coordinate of last click |
| `lastClickY` | `double` | Y coordinate of last click |
| `clickCount` | `int` | Click count (1=single, 2=double, 3=triple) |

### 2.6 Font System

| Variable | Type | Purpose |
|----------|------|---------|
| `ft` | `FT_Library` | FreeType library instance |
| `face` | `FT_Face` | Primary font face (proportional) |
| `monoFace` | `FT_Face` | Monospace font face for code |
| `fontLoaded` | `bool` | Whether fonts loaded successfully |

### 2.7 Rendering Components

| Variable | Type | Purpose |
|----------|------|---------|
| `markdownRenderer` | `unique_ptr<MarkdownRenderer>` | Markdown rendering pipeline |
| `textBuffer` | `unique_ptr<TextBuffer>` | Text buffer for markdown parser |
| `uiRenderer` | `unique_ptr<BatchRenderer>` | Batch renderer for UI elements |
| `uiAtlas` | `unique_ptr<GlyphAtlas>` | Glyph atlas for UI text |
| `uiRendererInitialized` | `bool` | Whether UI renderer is ready |

### 2.8 Undo System

| Variable | Type | Purpose |
|----------|------|---------|
| `undoStack` | `vector<UndoState>` | Stack of previous document states |
| `MAX_UNDO` | `const int` | Maximum undo history size (100) |

### 2.9 Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `TOOLBAR_HEIGHT` | 40 | Height of the toolbar in pixels |
| `INPUT_BUFFER_SIZE` | 10MB | Maximum document size |
| `MAX_UNDO` | 100 | Maximum undo stack depth |

## 3. Key Methods and Their Responsibilities

### 3.1 Lifecycle Methods

#### `Engine::Engine()`
Constructor that:
- Initializes all state variables to defaults
- Allocates 10MB input buffer
- Creates `MarkdownRenderer` and `TextBuffer` instances

#### `Engine::~Engine()`
Destructor that:
- Frees input buffer
- Releases FreeType font faces and library

#### `bool Engine::initialize()`
Initialization method that:
1. Enables OpenGL blending for transparency
2. Initializes FreeType library
3. Loads platform-specific fonts (Helvetica/Arial on macOS, Noto Sans on Linux/Web)
4. Loads monospace font for code rendering
5. Passes font faces to markdown renderer
6. Returns success/failure status

### 3.2 Rendering Methods

#### `void Engine::render(int width, int height)`
Main render loop that:
1. Clamps scroll offset to valid range
2. Sets up OpenGL viewport and clears to white
3. Renders the toolbar (fixed position)
4. Sets up scissor test for content clipping
5. Updates caret blink animation (320ms cycle)
6. Renders content in appropriate mode (raw or rendered)
7. Updates caret animation position
8. Draws scrollbar if content exceeds viewport

#### `void Engine::renderToolbar(int width)`
Renders the fixed toolbar containing:
- Format buttons: Bold (B), Italic (I), H1, H2, H3, Link
- Raw mode toggle button (right side)
- Button backgrounds, borders, and labels

#### `void Engine::renderRawText(int width, int height)`
Raw mode rendering with:
- Syntax highlighting for markdown elements
- Word wrapping at viewport edges
- Selection highlighting
- Cursor rendering
- Color coding: headings (blue), code (red), links (green), bold/italic markers (purple), list markers (orange), blockquotes (green)

### 3.3 Input Handling Methods

#### `void Engine::handleKeyboard(int key, int scancode, int action, int mods)`
Processes keyboard input:

**Shortcuts (Cmd/Ctrl + key):**
- `W`: Close window
- `A`: Select all
- `C`: Copy selection
- `V`: Paste
- `Z`: Undo
- `R`: Toggle raw mode

**Navigation:**
- Arrow keys: Move cursor (with Shift for selection)
- Cmd/Ctrl + Left/Right: Jump to line start/end
- Cmd/Ctrl + Up/Down: Jump to document start/end
- Alt + Left/Right: Move by word
- Home/End: Document start/end

**Editing:**
- Backspace: Delete character/selection (Cmd+Backspace deletes to line start, Alt+Backspace deletes word)
- Delete: Delete forward character/selection
- Enter: Insert newline
- Character keys: Insert character

#### `void Engine::handleMouse(int button, int action, int mods, double x, double y)`
Processes mouse clicks:
1. Checks for toolbar clicks first
2. Detects multi-clicks (double/triple) within 400ms and 5px
3. Performs hit testing to determine cursor position
4. Handles link clicks (opens URL in browser)
5. Single click: Position cursor
6. Double click: Select word
7. Triple click: Select line (including newline)

#### `void Engine::handleMouseMove(double x, double y)`
Handles mouse drag for selection:
- Updates cursor position and selection end while left button held
- Performs hit testing in appropriate mode (raw or rendered)

#### `void Engine::handleScroll(double xoffset, double yoffset)`
Processes scroll wheel input:
- Multiplies by 40 pixels per scroll unit
- Updates `scrollOffset`

### 3.4 Text Navigation Methods

#### `void Engine::moveCursor(int delta, bool extendSelection)`
Moves cursor by character offset:
- With selection: Extends selection from anchor point
- Without selection: Cancels selection and jumps to start/end of former selection
- Updates goal column for vertical navigation

#### `void Engine::moveCursorByWord(int direction, bool extendSelection)`
Moves cursor to next word boundary:
- Forward: Skips current word, then whitespace
- Backward: Skips whitespace, then previous word
- Optionally extends selection

#### `void Engine::moveCursorVertically(int direction, bool extendSelection)`
Moves cursor up/down by line:
- Maintains goal column across lines of different lengths
- Handles line wrapping in layout
- Ensures cursor visibility after move

#### `int Engine::findWordBoundary(int pos, int direction)`
Finds next word boundary in given direction.

#### `int Engine::findLineStart(int pos)` / `int Engine::findLineEnd(int pos)`
Finds the start/end of the line containing `pos`.

#### `int Engine::getColumnInLine(int pos)`
Returns the column (character offset from line start) of position.

#### `int Engine::findPositionInLine(int lineStart, int column)`
Finds position at given column in line, clamping to line length.

### 3.5 Text Editing Methods

#### `void Engine::insertChar(char c)`
Inserts single character:
1. Saves undo state
2. Replaces selection if present
3. Inserts character at cursor
4. Updates markdown renderer
5. Ensures cursor visible

#### `void Engine::insertText(const std::string& text)`
Inserts multi-character string (used for paste, drag-and-drop):
- Same flow as insertChar but handles multiple characters

#### `void Engine::deleteChar()`
Deletes character before cursor.

#### `void Engine::deleteWordBackward()`
Deletes from cursor to previous word boundary (Alt+Backspace).

### 3.6 Clipboard Operations

#### `void Engine::selectAll()`
Selects entire document content.

#### `void Engine::copySelection()`
Copies selected text to system clipboard using `Clipboard::setText()`.

#### `void Engine::paste()`
Pastes from system clipboard:
1. Gets clipboard content via `Clipboard::getText()`
2. Replaces selection if present
3. Inserts clipboard text at cursor

### 3.7 Formatting Methods (Toolbar Actions)

#### `void Engine::applyBold()`
- With selection: Wraps in `**...**`
- Without selection: Inserts `****` and positions cursor between

#### `void Engine::applyItalic()`
- With selection: Wraps in `*...*`
- Without selection: Inserts `**` and positions cursor between

#### `void Engine::applyHeading(int level)`
- Adds/replaces heading prefix (`#`, `##`, `###`) at line start
- Removes existing heading markers first

#### `void Engine::applyLink()`
- With selection: Wraps as `[selected](url)` and selects "url" placeholder
- Without selection: Inserts `[text](url)` and selects "text" placeholder

#### `void Engine::wrapSelection(const std::string& before, const std::string& after)`
Helper that wraps current selection with prefix and suffix strings.

### 3.8 Utility Methods

#### `void Engine::ensureCursorVisible()`
Adjusts scroll offset to keep cursor within visible viewport with margin.

#### `void Engine::updateCaretAnimation()`
Updates animated caret position using lerp (40% interpolation factor).

#### `void Engine::saveUndoState()`
Pushes current document state to undo stack (limited to MAX_UNDO entries).

#### `void Engine::undo()`
Restores previous document state from undo stack.

#### `void Engine::openUrl(const std::string& url)`
Opens URL in system browser using `open` command (macOS).

#### `bool Engine::isOverLink(double x, double y)`
Returns whether mouse coordinates are over a clickable link.

### 3.9 Hit Testing

#### `int Engine::hitTestRaw(float x, float y)`
Performs hit testing in raw mode:
- Builds line structure with word wrapping
- Finds clicked line by y coordinate
- Calculates character position from x offset

#### `float Engine::getCursorYRaw()`
Returns cursor Y position in raw mode (for scroll synchronization).

## 4. State Management

### 4.1 Document State
- **Input Buffer**: Raw markdown text stored in `inputBuffer[0..inputLength]`
- **Dirty Flag**: Set on any edit, cleared by `markClean()`
- **Undo Stack**: Vector of `UndoState` containing text snapshots and cursor positions

### 4.2 Editor State
- **Cursor Position**: Single integer offset into buffer
- **Selection**: Defined by `selectionStart`, `selectionEnd`, and `hasSelection`
- **Goal Column**: Preserved across vertical navigation for consistent column positioning

### 4.3 View State
- **Scroll Offset**: Pixel offset from top of document
- **Mode Toggle**: `showRaw` switches between rendered and raw views
- **Caret Animation**: Smooth interpolation between target positions

### 4.4 State Synchronization
When document content changes, the engine:
1. Updates `inputBuffer` and `inputLength`
2. Creates new `TextBuffer` with updated content
3. Passes to `MarkdownRenderer` via `setTextBuffer()`
4. Marks document as dirty

## 5. Input Handling

### 5.1 Keyboard Input Flow
```
GLFW Key Callback -> Engine::handleKeyboard()
    |
    +-- Check modifiers (Shift, Alt, Cmd/Ctrl)
    +-- Route to appropriate handler:
        +-- Shortcuts (copy, paste, undo, etc.)
        +-- Navigation (arrows, home, end)
        +-- Editing (backspace, delete, enter)
        +-- Character input (letters, numbers, symbols)
```

### 5.2 Mouse Input Flow
```
GLFW Mouse Callback -> Engine::handleMouse()
    |
    +-- Check toolbar click -> handleToolbarClick()
    +-- Detect multi-click (double/triple)
    +-- Hit test position:
        +-- Raw mode: hitTestRaw()
        +-- Rendered mode: markdownRenderer->hitTest()
    +-- Update cursor/selection
    +-- Handle link clicks -> openUrl()
```

### 5.3 Platform-Agnostic Design
- Both `GLFW_MOD_CONTROL` and `GLFW_MOD_SUPER` are checked for shortcuts
- Supports cross-platform keyboard conventions (Cmd on macOS, Ctrl on Linux/Windows)

## 6. Integration with Other Components

### 6.1 MarkdownRenderer
- Receives `TextBuffer` updates on content changes
- Provides `rawToDOM()` for cursor position mapping
- Performs hit testing via `hitTest()`
- Reports cursor position via `getCursorXY()`
- Returns content height for scroll calculations
- Handles link detection via `getLinkAtPosition()`

### 6.2 TextBuffer
- Holds document text content
- Cloned and passed to MarkdownRenderer on updates

### 6.3 BatchRenderer (uiRenderer)
- Renders toolbar buttons and backgrounds
- Draws scrollbar thumb
- Renders raw mode text with syntax highlighting

### 6.4 GlyphAtlas (uiAtlas)
- Caches rendered glyphs for UI text
- Used by BatchRenderer for text drawing

### 6.5 Clipboard
- Static interface for platform clipboard operations
- `Clipboard::getText()` for paste
- `Clipboard::setText()` for copy

### 6.6 FreeType
- Font loading and glyph rendering
- Separate faces for proportional and monospace fonts

## 7. Notable Implementation Details

### 7.1 Buffer Management
- Fixed 10MB input buffer allocated at construction
- Uses `memmove()` for safe overlapping copies during insertion/deletion
- Null-terminated for safety

### 7.2 Caret Animation
- Smooth animation using linear interpolation (lerp)
- 40% interpolation factor per frame
- Snaps to target when within 0.5 pixels
- 320ms blink cycle

### 7.3 Multi-Click Detection
- 400ms timeout for multi-click detection
- 5 pixel tolerance for position
- Cycles: single -> double -> triple -> single

### 7.4 Scroll Behavior
- Content clipped below toolbar using scissor test
- 60px bottom margin for "breathing room"
- 50px margin for cursor visibility adjustments
- Scrollbar only shown when content exceeds viewport

### 7.5 Raw Mode Syntax Highlighting
Color scheme:
- Default text: dark gray (0.2, 0.2, 0.2)
- Headings (#): blue (0.0, 0.4, 0.8)
- Code/code blocks: red (0.7, 0.2, 0.2)
- Links: green (0.1, 0.5, 0.1)
- Bold/italic markers: purple (0.6, 0.3, 0.6)
- List markers: orange (0.8, 0.5, 0.0)
- Blockquotes: green (0.4, 0.6, 0.4)

### 7.6 Word Wrapping
- Both rendered and raw modes implement word wrapping
- Breaks at spaces when possible
- Falls back to hard breaks when no space found
- Consistent wrapping between hit testing and rendering

### 7.7 Undo System
- Saves complete document state before each edit
- Limited to 100 states (oldest removed when exceeded)
- Restores both text and cursor position

### 7.8 Platform Font Loading
```cpp
// macOS
fontPaths[] = { "/System/Library/Fonts/Helvetica.ttc", ... }
monoFontPaths[] = { "/System/Library/Fonts/Menlo.ttc", ... }

// Linux
fontPaths[] = { "fonts/NotoSans-Regular.ttf" }
monoFontPaths[] = { "fonts/NotoSansMono-Regular.ttf" }

// Emscripten (Web)
fontPaths[] = { "/fonts/NotoSans-Regular.ttf" }
monoFontPaths[] = { "/fonts/NotoSansMono-Regular.ttf" }
```

### 7.9 Toolbar Layout
- Fixed 40px height
- Left side: B, I, H1, H2, H3, Link buttons
- Right side: Raw mode toggle
- 8px spacing between buttons
- White button backgrounds with gray borders
