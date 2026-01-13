# Markdown Renderer Specification

## 1. Purpose and Overview

The `MarkdownRenderer` class is the central orchestrator for transforming raw markdown text into rendered visuals on screen. It implements a multi-stage rendering pipeline inspired by browser rendering engines (similar to Blink/WebKit), coordinating four distinct phases:

1. **Parsing** - Converts raw markdown text into a semantic DOM tree (`MarkdownObject`)
2. **Layout** - Transforms the DOM into a layout tree (`LayoutObject`) with computed positions and sizes
3. **Painting** - Generates a display list of drawing operations (`PaintOp`)
4. **Rasterization** - Executes the display list via OpenGL

The renderer maintains dirty flags (`needsReparse`, `needsRelayout`, `needsRepaint`) to optimize updates by only re-running necessary pipeline stages when content changes.

### Source Files

- **Header**: `/protos/a/src/engine/markdown_renderer.h`
- **Implementation**: `/protos/a/src/engine/markdown_renderer.cpp`

---

## 2. Class Structure and Responsibilities

### 2.1 Core Class: `MarkdownRenderer`

```cpp
class MarkdownRenderer {
    // Input
    std::unique_ptr<TextBuffer> textBuffer;
    CaretState caretState;

    // Pipeline components
    std::unique_ptr<MarkdownParser> parser;
    std::unique_ptr<LayoutEngine> layoutEngine;
    std::unique_ptr<Painter> painter;
    std::unique_ptr<Rasterizer> rasterizer;

    // Intermediate representations
    std::unique_ptr<MarkdownObject> objectTree;   // DOM tree
    std::unique_ptr<LayoutObject> layoutTree;     // Layout tree
    DisplayList displayList;                       // Paint operations

    // Dirty flags
    bool needsReparse;
    bool needsRelayout;
    bool needsRepaint;
    Size lastViewportSize;
};
```

### 2.2 CaretState Structure

The `CaretState` structure tracks cursor and selection state for rendering:

```cpp
struct CaretState {
    int cursorPosition;        // Current cursor position (DOM coordinates)
    int selectionStart;        // Selection start (DOM coordinates)
    int selectionEnd;          // Selection end (DOM coordinates)
    bool hasSelection;         // Whether text is selected
    bool caretVisible;         // For blinking cursor animation
    float animatedCaretX;      // Animated X position for smooth movement
    float animatedCaretY;      // Animated Y position
    bool useAnimatedPosition;  // Whether to use animated position
};
```

### 2.3 Pipeline Components

| Component | Class | Responsibility |
|-----------|-------|----------------|
| Parser | `MarkdownParser` | Parse raw text into `MarkdownObject` DOM tree |
| Layout Engine | `LayoutEngine` | Create `LayoutObject` tree with computed geometry |
| Painter | `Painter` | Generate `DisplayList` of paint operations |
| Rasterizer | `Rasterizer` | Execute paint operations via OpenGL |

---

## 3. DOM Traversal and Position Mapping

### 3.1 Collecting Text Objects

The renderer uses recursive helper functions to traverse the DOM tree and collect text objects:

```cpp
static void collectTextObjects(const MarkdownObject* obj,
                               std::vector<const MarkdownObject*>& out) {
    if (!obj) return;
    if (obj->getType() == MarkdownObjectType::Text) {
        out.push_back(obj);
    }
    for (const auto& child : obj->getChildren()) {
        collectTextObjects(child.get(), out);
    }
}
```

### 3.2 DOM Position Calculation

The renderer maintains two coordinate systems:

1. **Raw Positions** - Byte offsets in the original markdown text (includes syntax characters like `#`, `**`, etc.)
2. **DOM Positions** - Character offsets in the rendered/visible text (excludes syntax)

Key methods for position conversion:

| Method | Direction | Description |
|--------|-----------|-------------|
| `domToRaw(int domPos)` | DOM -> Raw | Converts DOM position to raw text offset |
| `rawToDOM(int rawPos)` | Raw -> DOM | Converts raw text offset to DOM position |
| `getTotalDOMLength()` | N/A | Returns total length of visible text |

### 3.3 Position Mapping Algorithm

The `domToRaw` method walks through text objects linearly:

1. Iterate through all text objects in order
2. Track cumulative DOM position
3. When target position falls within an object's range, compute: `rawStart + localOffset`
4. Empty lines are treated as occupying 1 DOM position

The `rawToDOM` method performs the inverse:

1. Check if raw position falls before first text object (return 0)
2. For each text object, check if raw position is within `[rawStart, rawEnd]`
3. Handle gaps between objects (markdown syntax) by returning end of previous object

---

## 4. Rendering Logic for Different Element Types

### 4.1 Supported Markdown Object Types

```cpp
enum class MarkdownObjectType {
    Document,     // Root container
    Heading,      // H1-H6 headers
    Paragraph,    // Regular text blocks
    Image,        // Inline/block images
    Bold,         // **bold** or __bold__
    Italic,       // *italic* or _italic_
    Underline,    // Underlined text
    Link,         // [text](url)
    BlockQuote,   // > quoted text
    CodeBlock,    // ```code```
    Frontmatter,  // YAML frontmatter
    Equation,     // LaTeX equations
    List,         // Ordered/unordered lists
    ListItem,     // Individual list items
    Table,        // Markdown tables
    TableRow,     // Table rows
    TableCell,    // Table cells
    Text          // Leaf text nodes
};
```

### 4.2 Layout Object Hierarchy

| Layout Type | Base Class | DOM Length | Notes |
|-------------|------------|------------|-------|
| `BlockLayoutObject` | `LayoutObject` | 0 | Block-level container |
| `InlineLayoutObject` | `LayoutObject` | 0 | Inline container |
| `TextLayoutObject` | `LayoutObject` | text.length() | Text content with glyph runs |
| `ImageLayoutObject` | `LayoutObject` | 1 | Atomic element |
| `TableLayoutObject` | `LayoutObject` | 0 | Table container |
| `TableRowLayoutObject` | `LayoutObject` | 0 | Row container |
| `TableCellLayoutObject` | `LayoutObject` | 0 | Cell container |
| `ListItemLayoutObject` | `LayoutObject` | 0 | List item with marker |

### 4.3 Text Styling

Inline formatting is tracked via style ranges:

```cpp
enum class TextStyle : uint8_t {
    Normal     = 0,
    Bold       = 1 << 0,
    Italic     = 1 << 1,
    Code       = 1 << 2,
    BoldItalic = Bold | Italic
};

struct InlineStyleRange {
    int startChar;
    int endChar;
    TextStyle style;
};
```

### 4.4 Link Handling

Links are tracked via `InlineLinkRange` structures attached to paragraphs:

```cpp
struct InlineLinkRange {
    int startChar;    // Start character in display text
    int endChar;      // End character in display text
    std::string url;  // Target URL
};
```

The `getLinkAtPosition(float x, float y)` method performs hit testing to find clickable links.

---

## 5. Integration with Layout Engine

### 5.1 Pipeline Flow

```
TextBuffer
    |
    v
MarkdownParser::parse()  -->  MarkdownObject (DOM tree)
    |
    v
LayoutEngine::createLayoutTree()  -->  LayoutObject (Layout tree)
    |
    v
LayoutEngine::performLayout()  -->  Geometry computed
    |
    v
Painter::paint()  -->  DisplayList
    |
    v
Rasterizer::rasterize()  -->  OpenGL rendering
```

### 5.2 Font Configuration

The renderer passes font faces to the layout engine:

```cpp
void setFontFace(FT_Face face);      // Regular font
void setMonoFontFace(FT_Face face);  // Monospace font (for code)
```

### 5.3 Dirty Flag Cascade

Changes cascade through the pipeline:

| Change | needsReparse | needsRelayout | needsRepaint |
|--------|--------------|---------------|--------------|
| Text content modified | Yes | Yes | Yes |
| Viewport size changed | No | Yes | Yes |
| Font changed | No | Yes | Yes |
| Caret position changed | No | No | Yes |
| Caret visibility toggled | No | No | No* |

*Caret visibility changes only trigger repaint if position also changed.

### 5.4 Layout Tree Traversal

For cursor positioning and hit testing, the renderer collects `TextLayoutObject` instances with their DOM positions:

```cpp
static void collectTextLayoutsWithPos(
    const LayoutObject* obj,
    std::vector<std::pair<const TextLayoutObject*, int>>& out,
    int& currentDOMPos
);
```

---

## 6. Notable Implementation Details

### 6.1 Hit Testing Algorithm

The `hitTest(float x, float y)` method converts screen coordinates to a raw cursor position:

1. **Collect all TextLayoutObjects** with their DOM start positions
2. **Find exact match** - Check if (x, y) falls within a text line's bounding box
3. **Fallback: Same-line search** - If no exact match, find layouts on the same Y line and pick closest by X
4. **Fallback: Closest Y** - If still no match, find the line with minimum Y distance
5. **Character-level hit test** - Within the found line, iterate characters using `getCharXOffsetInLine()` to find the exact character position
6. **Convert to raw position** - Use `domToRaw()` to return the raw text offset

### 6.2 Cursor Positioning

Two methods provide cursor geometry:

- `getCursorY(int domPos)` - Returns Y coordinate (bottom of line) for auto-scroll
- `getCursorXY(int domPos, float& outX, float& outY)` - Returns full cursor position

Both methods:
1. Walk the layout tree to find the containing `TextLayoutObject`
2. Compute local offset within that object
3. Use line information and character offsets to compute final coordinates

### 6.3 Multi-line Text Handling

`TextLayoutObject` maintains line information for wrapped text:

```cpp
struct LineInfo {
    int startChar;   // First character index
    int endChar;     // Last character index (exclusive)
    float yOffset;   // Vertical offset from layout origin
    float width;     // Line width
};
```

Character positioning within lines uses:
- `getLineForChar(int charIndex)` - Find which line contains a character
- `getCharXOffsetInLine(int charIndex)` - Get X offset relative to line start

### 6.4 Empty Line Handling

Empty lines (blank paragraphs) are special-cased:
- They occupy exactly 1 DOM position
- `domToRaw()` returns `rawStart` for empty lines
- This ensures cursor can be placed on blank lines

### 6.5 Display List Operations

The painter generates these operation types:

```cpp
enum class PaintOpType {
    DrawRect,          // Filled rectangle (backgrounds)
    DrawText,          // Text with font/style
    DrawImage,         // Image rendering
    SetClip,           // Push clip rectangle
    RestoreClip,       // Pop clip rectangle
    DrawDebugBorder,   // Debug visualization
    DrawCaret,         // Text cursor
    DrawSelectionRect, // Selection highlight
    DrawLine           // Lines (underlines, blockquote bars)
};
```

### 6.6 Rendering Optimization

The renderer includes several optimizations:

1. **Dirty flag system** - Only re-run necessary pipeline stages
2. **Viewport change detection** - Relayout only if viewport dimensions change
3. **Caret visibility optimization** - Pure visibility changes don't trigger repaint
4. **Cached layout tree** - Layout tree persists between frames

### 6.7 Content Height Calculation

```cpp
float getContentHeight() const {
    if (layoutTree) {
        return layoutTree->getRect().size.height;
    }
    return 0;
}
```

This is used for scroll calculations to determine total scrollable content.

---

## 7. Public API Summary

### Configuration

| Method | Description |
|--------|-------------|
| `setTextBuffer(buffer)` | Set the markdown text source |
| `setCaretState(state)` | Update cursor/selection state |
| `setFontFace(face)` | Set regular font |
| `setMonoFontFace(face)` | Set monospace font |

### Rendering

| Method | Description |
|--------|-------------|
| `render(viewportSize, scrollOffsetY)` | Execute full render pipeline |
| `parseMarkdown()` | Manual: parse only |
| `performLayout(availableSpace)` | Manual: layout only |
| `paint()` | Manual: paint only |
| `rasterize(viewportSize, scrollOffsetY)` | Manual: rasterize only |

### Inspection

| Method | Returns | Description |
|--------|---------|-------------|
| `getObjectTree()` | `MarkdownObject*` | DOM tree |
| `getLayoutTree()` | `LayoutObject*` | Layout tree |
| `getDisplayList()` | `DisplayList&` | Paint operations |
| `getContentHeight()` | `float` | Total content height |

### Position Mapping

| Method | Description |
|--------|-------------|
| `getTotalDOMLength()` | Total visible text length |
| `domToRaw(domPos)` | Convert DOM position to raw offset |
| `rawToDOM(rawPos)` | Convert raw offset to DOM position |
| `hitTest(x, y)` | Screen coords to raw position |
| `getCursorY(domPos)` | Get cursor Y for scroll |
| `getCursorXY(domPos, x, y)` | Get full cursor position |
| `getLinkAtPosition(x, y)` | Get link URL at coords |
