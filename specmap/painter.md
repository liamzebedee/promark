# Painter Component Specification

## 1. Purpose and Overview

The Painter component is responsible for converting a laid-out document tree (LayoutObjects) into a list of primitive drawing operations (DisplayList). It acts as the bridge between the layout engine and the rasterizer, implementing a display list rendering architecture that separates visual representation from actual pixel rendering.

### Key Responsibilities

- Traverse the layout tree and generate appropriate paint operations for each element
- Handle text rendering with support for styled segments (bold, italic, code) and links
- Paint caret (text cursor) at the correct position with animation support
- Paint text selection highlights across potentially wrapped lines
- Generate debug borders when DEBUG=1 environment variable is set
- Apply element-specific styling (backgrounds, borders, markers)

### Architecture Position

```
LayoutEngine -> Painter -> Rasterizer
                  |
                  v
             DisplayList
```

The Painter receives:
- `LayoutObject*` - Root of the layout tree
- `CaretState*` - Cursor position and selection state
- `const char* text` - Raw text buffer
- `int textLength` - Length of the text buffer

It produces:
- `DisplayList` - A vector of unique pointers to paint operations

---

## 2. Paint Operation Types

The paint system defines nine operation types through the `PaintOpType` enum:

| Type | Class | Purpose |
|------|-------|---------|
| `DrawRect` | `DrawRectOp` | Filled rectangles (backgrounds, bullets) |
| `DrawText` | `DrawTextOp` | Text with position, color, size, style, and font type |
| `DrawImage` | `DrawImageOp` | Images with destination rect and path/URI |
| `DrawLine` | `DrawLineOp` | Lines with start, end, thickness (underlines, borders) |
| `DrawCaret` | `DrawCaretOp` | Text cursor (vertical line) |
| `DrawSelectionRect` | `DrawSelectionRectOp` | Selection highlight rectangles |
| `DrawDebugBorder` | `DrawDebugBorderOp` | Debug outline for layout debugging |
| `SetClip` | `SetClipOp` | Set clipping rectangle |
| `RestoreClip` | `RestoreClipOp` | Restore previous clipping state |

### Common Data Structures

```cpp
struct Color {
    uint8_t r, g, b, a;  // RGBA, alpha defaults to 255 (opaque)
};

struct Point { float x, y; };
struct Size { float width, height; };
struct Rect { Point position; Size size; };
```

### DrawTextOp Details

The text operation is the most complex, carrying:
- `Point position` - Baseline position (x, y)
- `std::string text` - Text content
- `Color color` - Text color
- `float fontSize` - Font size in pixels (default 16.0f)
- `TextStyle style` - Normal, Bold, Italic, BoldItalic, Code (bitmask)
- `bool monospace` - Whether to use monospace font

### DisplayList

```cpp
using DisplayList = std::vector<std::unique_ptr<PaintOp>>;
```

Operations are stored in order and executed sequentially by the rasterizer. Paint order is:
1. Selection rectangles (behind text)
2. Layout tree (backgrounds, text, images, borders)
3. Caret (on top)

---

## 3. Painter Class Structure

### Public Interface

```cpp
class Painter {
public:
    Painter();
    ~Painter();

    DisplayList paint(const LayoutObject* layoutRoot,
                      const CaretState* caret = nullptr,
                      const char* text = nullptr,
                      int textLength = 0);
};
```

### Private Methods

| Method | Purpose |
|--------|---------|
| `paintLayoutObject()` | Recursive tree traversal |
| `paintText()` | Text rendering with style segments |
| `paintImage()` | Image rendering |
| `paintBackground()` | Background rectangles |
| `paintBorder()` | Border painting (stub) |
| `paintDebugBorder()` | Debug mode borders |
| `paintBlockQuoteBar()` | Vertical gray bar for blockquotes |
| `paintLinkUnderline()` | Underlines for hyperlinks |
| `paintTable()` | Table borders and column separators |
| `paintTableRow()` | Row backgrounds and separators |
| `paintTableCell()` | Cell-specific styling (stub) |
| `paintListItem()` | Bullet points and ordered markers |
| `paintCaret()` | Text cursor |
| `paintSelection()` | Selection highlight rectangles |

### Helper Methods

| Method | Purpose |
|--------|---------|
| `findLayoutForPosition()` | Map DOM position to layout object |
| `collectContentLayouts()` | Gather all content-bearing layouts in order |
| `computeSelectionRect()` | Calculate selection rect for a layout segment |
| `computeXForOffset()` | Calculate X position for text offset |
| `isInsideLink()` | Check if layout is inside a link |
| `getTextColor()` | Determine text color by object type |
| `getBackgroundColor()` | Determine background color by object type |

---

## 4. Caret and Selection Rendering

### CaretState Structure

```cpp
struct CaretState {
    int cursorPosition;      // Absolute position in document
    int selectionStart;      // Selection anchor
    int selectionEnd;        // Selection extent
    bool hasSelection;       // Whether selection exists
    bool caretVisible;       // For blinking (handled by rasterizer)
    float animatedCaretX;    // Animated X position
    float animatedCaretY;    // Animated Y position
    bool useAnimatedPosition; // Use animated coords instead of calculated
};
```

### Caret Rendering Process

1. **Find Layout Object**: Use `findLayoutForPosition()` to locate the layout object containing the cursor position and calculate the local offset within that object.

2. **Position Calculation**:
   - **Atomic elements** (images): Caret at left edge (offset 0) or right edge (offset > 0)
   - **Text elements**: Use glyph position data and line information to calculate precise X/Y coordinates

3. **Line-Aware Positioning**: For wrapped text, find which line contains the cursor using `getLineForChar()`, then calculate X offset within that line using `getCharXOffsetInLine()`.

4. **Animation Support**: If `useAnimatedPosition` is true, use pre-calculated animated coordinates instead of computed positions (enables smooth caret transitions).

5. **Generate Operation**: Create `DrawCaretOp` with position, height (font size), and color (black).

```cpp
// Caret is rendered as a thin vertical rectangle
// Height matches the font size of the containing text element
auto caretOp = std::make_unique<DrawCaretOp>(
    Point(caretX, caretY),
    caretHeight,  // Usually fontSize
    Color(0, 0, 0, 255)  // Black
);
```

### Selection Rendering Process

1. **Normalize Selection**: Ensure start < end regardless of selection direction.

2. **Collect Content Layouts**: Traverse tree to get all content-bearing layout objects in document order.

3. **Track Position**: Walk through layouts, tracking cumulative DOM position.

4. **Overlap Detection**: For each layout, check if selection range overlaps its position range.

5. **Multi-Line Handling**: For wrapped text, iterate through lines and create separate selection rectangles for each affected line.

6. **Generate Operations**: Create `DrawSelectionRectOp` for each selection segment.

Selection color: `Color(173, 214, 255, 180)` - Light blue with transparency

```cpp
// Selection rectangles are painted FIRST (behind text)
// Multiple rectangles are generated for multi-line selections
for (int lineIdx = startLine; lineIdx <= endLine; lineIdx++) {
    Rect selRect(x1, yOffset, x2 - x1, lineHeight);
    auto selOp = std::make_unique<DrawSelectionRectOp>(selRect, selColor);
    displayList.push_back(std::move(selOp));
}
```

### DOMPositionResult Structure

```cpp
struct DOMPositionResult {
    const LayoutObject* layout;  // Containing layout object
    int localOffset;             // Offset within that object
    bool isAtomicBoundary;       // True for atomic elements (images)
};
```

---

## 5. Coordinate System and Positioning

### Coordinate Origin

- Origin (0, 0) is at the **top-left** of the viewport
- X increases rightward
- Y increases downward

### Text Positioning

- Text operations specify the **baseline** position, not top-left
- Y coordinate = rect.position.y + lineOffset + fontSize
- Multi-line text uses `yOffset` from `LineInfo` for each line

### Character Position Calculation

The `TextLayoutObject` provides several methods for character positioning:

| Method | Description |
|--------|-------------|
| `getCharXOffset(index)` | Cumulative X offset after character at index |
| `getLineForChar(charIndex)` | Which line (index) contains the character |
| `getCharXOffsetInLine(charIndex)` | X offset relative to line start |

### Layout Rect Structure

Each `LayoutObject` has a `Rect` describing its bounding box:
- `position.x, position.y` - Top-left corner
- `size.width, size.height` - Dimensions

### Scroll Handling

Scroll offset is NOT applied by the Painter. The raw document coordinates are used in the DisplayList. The Rasterizer receives `scrollOffsetY` and applies the transformation during rendering.

---

## 6. Integration with Rasterizer

### DisplayList Consumption

The `Rasterizer::rasterize()` method consumes the DisplayList:

```cpp
void Rasterizer::rasterize(const DisplayList& displayList,
                           const Rect& viewport,
                           float scrollOffsetY,
                           bool caretVisible);
```

### Operation Execution

Each operation type has a corresponding execute method in the Rasterizer:

| Paint Op | Execute Method | Implementation |
|----------|----------------|----------------|
| `DrawRect` | `executeDrawRect()` | `batchRenderer->drawRect()` |
| `DrawText` | `executeDrawText()` | `batchRenderer->drawText()` with font selection |
| `DrawImage` | `executeDrawImage()` | Texture loading + `batchRenderer->drawImage()` |
| `DrawCaret` | `executeDrawCaret()` | Thin rectangle (2px wide) |
| `DrawSelectionRect` | `executeDrawSelectionRect()` | `batchRenderer->drawRect()` |
| `DrawLine` | `executeDrawLine()` | Thin rectangle approximation |
| `DrawDebugBorder` | `executeDrawDebugBorder()` | Four thin rectangles |
| `SetClip` | `executeSetClip()` | `glScissor()` |
| `RestoreClip` | `executeRestoreClip()` | `glDisable(GL_SCISSOR_TEST)` |

### Caret Visibility

The Rasterizer handles caret blinking by checking the `caretVisible` parameter:

```cpp
case PaintOpType::DrawCaret:
    if (caretVisible) {
        executeDrawCaret(static_cast<const DrawCaretOp&>(*op));
    }
    break;
```

This allows the Painter to always generate caret operations, with visibility controlled at render time.

### Batch Rendering

The Rasterizer uses a `BatchRenderer` to efficiently combine multiple draw calls, reducing OpenGL state changes.

---

## 7. Notable Implementation Details

### Paint Order

The `paint()` method establishes a specific order:

1. **Selection first** - Painted behind all content
2. **Layout tree** - Content in document order
3. **Caret last** - Always visible on top

### Recursive Layout Painting

`paintLayoutObject()` follows this pattern for each node:

1. Paint background (if non-transparent)
2. Paint special decorations (blockquote bar)
3. Paint content (text, image, table, list marker)
4. Paint border (stub)
5. Paint debug border (if DEBUG=1)
6. Recurse into children

### Text Style Segmentation

Text rendering handles multiple styles within a single text object:

1. Pre-compute character-to-style mapping (O(n) space, O(1) lookup)
2. Pre-compute character-to-link mapping
3. Walk through lines, breaking at style/link boundaries
4. Generate separate `DrawTextOp` for each styled segment

### Color Constants

| Element | Color |
|---------|-------|
| Default text | Black (0, 0, 0) |
| Links | Blue (0, 102, 204) |
| Inline code | Reddish (200, 50, 50) |
| Selection | Light blue (173, 214, 255, 180) |
| Code block background | Light gray (240, 240, 240) |
| Frontmatter background | Warm cream (255, 250, 230) |
| Blockquote background | Very light gray (250, 250, 250) |
| Blockquote bar | Gray (200, 200, 200) |
| Table borders | Light gray (200, 200, 200) |
| Table header background | Light gray (245, 245, 245) |
| List markers | Dark gray (60, 60, 60) |
| Debug borders | Magenta (255, 0, 255) |

### Link Underlines

Links are rendered with:
- Blue text color
- 1px underline 2px below baseline
- Underlines are drawn as `DrawLineOp` for each line segment

### List Item Markers

- **Bullets**: Small filled rectangle (6x6 pixels)
- **Ordered**: Text marker (e.g., "1.", "a.") using `DrawTextOp`
- Marker position: Left of text indent, baseline-aligned

### Debug Mode

When `DEBUG=1` environment variable is set, magenta borders are drawn around all layout objects for visualization.

### UTF-8 Support

The painter uses UTF-8 utilities (`utf8::length()`, `utf8::substr()`) for correct character counting and substring extraction, ensuring proper handling of multi-byte characters.

### Atomic Elements

Images are treated as atomic elements:
- DOM length of 1
- Caret can only be at left or right edge
- Selection covers entire element

### Fallback Behavior

- Empty lines array: Render full text as single line
- Unknown layout types: Skip content painting, continue to children
- Border painting: Not yet implemented (stub method)
