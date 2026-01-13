# Layout Engine Specification

## 1. Purpose and Overview

The Layout Engine is responsible for transforming the parsed Markdown DOM tree into a positioned layout tree that can be painted and rasterized. It serves as the bridge between the semantic structure produced by the Markdown parser and the visual representation rendered by the Painter and Rasterizer.

### Primary Responsibilities

1. **Tree Transformation**: Convert `MarkdownObject` DOM nodes into corresponding `LayoutObject` nodes
2. **Position Calculation**: Compute absolute positions (x, y) and sizes (width, height) for all elements
3. **Text Shaping**: Use FreeType to compute glyph metrics and character positions
4. **Line Breaking**: Wrap text content to fit within available width
5. **Font Management**: Provide appropriate font faces for regular and monospace text

### Architecture Position

```
TextBuffer -> MarkdownParser -> [DOM Tree]
                                    |
                                    v
                             LayoutEngine -> [Layout Tree]
                                                  |
                                                  v
                                              Painter -> [Display List]
                                                              |
                                                              v
                                                         Rasterizer -> [Pixels]
```

---

## 2. Layout Object Types and Hierarchy

### Core Geometry Types

Defined in `layout_objects.h`:

```cpp
struct Point { float x, y; };
struct Size { float width, height; };
struct Rect { Point position; Size size; };
```

### Layout Flow Types

```cpp
enum class LayoutFlow {
    Block,   // Vertical stacking (paragraphs, headings, lists)
    Inline   // Horizontal flow with wrapping (text, links, emphasis)
};
```

### Layout Object Class Hierarchy

```
LayoutObject (base class)
    |
    +-- BlockLayoutObject      // Document, Paragraph, Heading, BlockQuote, CodeBlock, Frontmatter, List
    |
    +-- InlineLayoutObject     // Bold, Italic, Underline, Link
    |
    +-- TextLayoutObject       // Text content with shaping and line breaking
    |
    +-- ImageLayoutObject      // Images (atomic elements)
    |
    +-- TableLayoutObject      // Table container
    |       +-- TableRowLayoutObject
    |               +-- TableCellLayoutObject
    |
    +-- ListItemLayoutObject   // List items with markers and indentation
```

### LayoutObject Base Class

| Member | Type | Description |
|--------|------|-------------|
| `sourceObject` | `const MarkdownObject*` | Reference to corresponding DOM node |
| `flow` | `LayoutFlow` | Block or Inline flow type |
| `rect` | `Rect` | Computed position and size |
| `children` | `vector<unique_ptr<LayoutObject>>` | Child layout objects |
| `parent` | `LayoutObject*` | Parent pointer for font size inheritance |

### Key Virtual Methods

| Method | Description |
|--------|-------------|
| `computeIntrinsicSize()` | Calculate natural size without constraints |
| `layout(Size availableSpace)` | Perform layout given available space |
| `getFontSize()` | Get font size (inherited from parent for text) |
| `getDOMLength()` | Return DOM position length for cursor positioning |
| `isAtomic()` | True for elements that occupy exactly 1 DOM position (images) |

---

## 3. Layout Algorithm

### Main Entry Points

The `LayoutEngine` class provides two main methods:

```cpp
// Create layout tree from DOM tree
unique_ptr<LayoutObject> createLayoutTree(const MarkdownObject* objectTree);

// Compute positions and sizes
void performLayout(LayoutObject* layoutRoot, const Size& availableSpace);
```

### Tree Creation Phase

The `createLayoutTree()` method recursively walks the DOM tree and creates corresponding layout objects:

1. **Object Type Mapping**:
   - `Document`, `Paragraph`, `Heading`, `BlockQuote`, `CodeBlock`, `Frontmatter`, `List` -> `BlockLayoutObject`
   - `ListItem` -> `ListItemLayoutObject`
   - `Text` -> `TextLayoutObject`
   - `Image` -> `ImageLayoutObject`
   - `Table`, `TableRow`, `TableCell` -> Corresponding table layout objects
   - `Bold`, `Italic`, `Underline`, `Link` -> `InlineLayoutObject`

2. **Code Block Context**: Tracks whether we are inside a code block or frontmatter to assign monospace fonts to nested text objects.

3. **Child Recursion**: Creates layout objects for all children and establishes parent-child relationships.

### Layout Phase

The `performLayout()` method dispatches based on flow type and object characteristics:

```cpp
void performLayout(LayoutObject* layoutRoot, const Size& availableSpace) {
    // Atomic elements (images) handle their own layout
    if (layoutRoot->isAtomic()) {
        layoutRoot->layout(availableSpace);
        return;
    }

    // Tables, list items handle their own child layout
    if (isTableOrListItem(layoutRoot)) {
        layoutRoot->layout(availableSpace);
        return;
    }

    // Dispatch by flow type
    if (layoutRoot->getFlow() == LayoutFlow::Block) {
        layoutBlockFlow(layoutRoot, availableSpace);
    } else {
        layoutInlineFlow(layoutRoot, availableSpace);
    }
}
```

---

## 4. Text Layout and Line Breaking

### TextLayoutObject

The `TextLayoutObject` class handles the most complex layout logic:

#### Data Structures

```cpp
struct GlyphRun {
    vector<uint32_t> glyphIds;
    vector<Point> positions;
    float width, height;
};

struct LineInfo {
    int startChar;   // First character index in this line
    int endChar;     // Past-the-end character index
    float yOffset;   // Y position relative to text block
    float width;     // Width of text on this line
};
```

#### Text Shaping (`shapeText()`)

1. **Style Detection**: Pre-computes which characters have inline code styling for O(1) lookup
2. **Font Selection**: Uses monospace font for inline code, regular font otherwise
3. **Glyph Metrics**: Uses FreeType to compute advance widths:
   ```cpp
   FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT);
   x += face->glyph->advance.x / 64.0f;  // 26.6 fixed-point
   ```
4. **UTF-8 Handling**: Properly decodes multi-byte UTF-8 sequences to Unicode code points
5. **Character Offsets**: Builds `charXOffsets` array with cumulative x positions

#### Line Breaking (`wrapText()`)

The algorithm performs soft line breaks:

1. **Scan Characters**: Iterate through code points, tracking cumulative width
2. **Word Boundaries**: Track last space position for word-based wrapping
3. **Forced Breaks**: Handle explicit `\n` characters immediately
4. **Width Check**: When `lineWidth > maxWidth`:
   - Prefer breaking at word boundary (after last space)
   - Fall back to character break if no word boundary
5. **Line Recording**: Store `LineInfo` with character range, y-offset, and width

```cpp
for (each character) {
    if (character == '\n') {
        // Force line break
        lines.push_back({lineStart, i, yOffset, width});
        lineStart = i + 1;
    }
    else if (lineWidth > maxWidth && i > lineStart) {
        // Soft wrap at word boundary or character
        if (lastWordEnd > lineStart) {
            lines.push_back({lineStart, lastWordEnd, yOffset, width});
            lineStart = lastWordEnd + 1;
        } else {
            lines.push_back({lineStart, i, yOffset, width});
            lineStart = i;
        }
    }
}
```

#### Character Position Queries

Used by the Painter for cursor positioning and hit testing:

| Method | Description |
|--------|-------------|
| `getCharCount()` | Total number of characters |
| `getCharXOffset(index)` | Cumulative X offset after character |
| `getLineForChar(charIndex)` | Which line contains a character |
| `getCharXOffsetInLine(charIndex)` | X offset relative to line start |

---

## 5. Block vs Inline Layout

### Block Flow Layout (`layoutBlockFlow()`)

Block elements stack vertically with margins:

1. **Margin Calculation**:
   - Document root: `DOCUMENT_MARGIN` (50px) on all sides
   - Block quotes: `BLOCKQUOTE_INDENT` (20px) left indent
   - Code blocks/frontmatter: `CODE_BLOCK_PADDING` (8px) internal padding

2. **Child Layout Loop**:
   ```cpp
   float currentY = marginTop;
   for (each child) {
       // Compute available space for child
       Size childSpace(availableWidth - marginLeft * 2, remainingHeight);

       // Recursively layout child
       performLayout(child, childSpace);

       // Position child
       child->setRect(Rect(marginLeft, currentY, childWidth, childHeight));

       // Propagate position to grandchildren
       propagatePositionToChildren(child, marginLeft, currentY);

       // Advance Y with block spacing
       currentY += childHeight + BLOCK_SPACING;
   }
   ```

3. **Empty Paragraph Handling**: Empty paragraphs get zero height but maintain position for cursor placement

4. **Position Propagation**: After positioning a child, its offset is added to all grandchildren to convert from relative to absolute coordinates

### Inline Flow Layout

Currently delegated to individual layout objects. The base `InlineLayoutObject::layout()` is a TODO placeholder for horizontal flow with line breaking.

For text, the `TextLayoutObject::layout()` handles inline text layout:
```cpp
void TextLayoutObject::layout(const Size& availableSpace) {
    availableWidth = availableSpace.width;
    shapeText();                          // Compute glyph positions
    wrapText(availableSpace.width);       // Perform line breaking
    Size textSize = computeIntrinsicSize();
    setRect(Rect(0, 0, availableSpace.width, textSize.height));
}
```

---

## 6. Integration with Markdown Renderer and Rasterizer

### MarkdownRenderer Integration

The `MarkdownRenderer` orchestrates the full rendering pipeline:

```cpp
class MarkdownRenderer {
    unique_ptr<LayoutEngine> layoutEngine;
    unique_ptr<LayoutObject> layoutTree;

    void performLayout(const Size& availableSpace) {
        // Create layout tree from DOM
        layoutTree = layoutEngine->createLayoutTree(objectTree.get());

        // Compute positions and sizes
        layoutEngine->performLayout(layoutTree.get(), availableSpace);
    }
};
```

### Dirty Tracking

The renderer tracks what needs updating:
- `needsReparse`: Source text changed, reparse markdown
- `needsRelayout`: Viewport size changed or fonts changed
- `needsRepaint`: Caret position or selection changed

### Painter Integration

The `Painter` walks the layout tree to generate paint operations:

```cpp
DisplayList Painter::paint(const LayoutObject* layoutRoot, ...) {
    DisplayList displayList;
    paintLayoutObject(layoutRoot, displayList);
    return displayList;
}

void Painter::paintLayoutObject(const LayoutObject* obj, DisplayList& list) {
    paintBackground(obj, list);

    if (auto* text = dynamic_cast<const TextLayoutObject*>(obj)) {
        paintText(text, list);
    } else if (auto* image = dynamic_cast<const ImageLayoutObject*>(obj)) {
        paintImage(image, list);
    }
    // ... etc

    for (const auto& child : obj->getChildren()) {
        paintLayoutObject(child.get(), list);
    }
}
```

### Hit Testing

The layout tree supports hit testing for cursor positioning:

```cpp
int MarkdownRenderer::hitTest(float x, float y) const {
    // Find TextLayoutObject containing click point
    // Check line Y ranges, then X positions
    // Return DOM position for cursor
}
```

---

## 7. Notable Implementation Details

### Font Size Inheritance

Text objects inherit font size from their parent layout object:

```cpp
float TextLayoutObject::getFontSize() const {
    if (parent) {
        return parent->getFontSize();
    }
    return Typography::BASE_FONT_SIZE;
}

float LayoutObject::getFontSize() const {
    if (sourceObject->getType() == MarkdownObjectType::Heading) {
        return Typography::headingSize(heading->getLevel());
    }
    return Typography::BASE_FONT_SIZE;
}
```

### Typography Constants

Defined in `typography.h`:

| Constant | Value | Description |
|----------|-------|-------------|
| `BASE_FONT_SIZE` | 28.0f | Body text size (Retina) |
| `SCALE_RATIO` | 1.2f | Heading scale ratio (minor third) |
| `BLOCK_SPACING` | 14.0f | Vertical space between blocks |
| `DOCUMENT_MARGIN` | 50.0f | Page margins |
| `BLOCKQUOTE_INDENT` | 20.0f | Blockquote left indent |
| `LIST_INDENT` | 24.0f | List item indentation per level |

### Image Size Computation

`ImageLayoutObject` lazily computes intrinsic size:

1. **File paths**: Uses stb_image to get dimensions without decoding
2. **Data URIs**: Decodes base64, then parses PNG/JPEG headers for dimensions
3. **Default**: Falls back to 200x150 if size cannot be determined

### Table Layout

Tables use equal-width columns:

```cpp
void TableLayoutObject::computeColumnWidths(float availableWidth) {
    float usableWidth = availableWidth - totalBorders;
    float columnWidth = usableWidth / columnCount;
    // All columns get equal width
}
```

Row layout positions cells with padding and passes column widths from parent table.

### List Item Layout

List items handle their own indentation:

```cpp
void ListItemLayoutObject::layout(const Size& availableSpace) {
    float indentWidth = LIST_INDENT * (indentLevel + 1);
    // Layout children at reduced width
    // Position children offset by indent
}
```

The marker (bullet or number) is rendered separately by the Painter based on `getMarkerType()` and `getMarkerText()`.

### DOM Position Tracking

For cursor positioning and selection, each layout object reports its DOM length:

- `TextLayoutObject`: Returns code point count (not byte count)
- `ImageLayoutObject`: Returns 1 (atomic element)
- Container objects: Return 0 (children contribute positions)

Empty text objects return 1 to ensure cursor can be positioned on empty lines.

### UTF-8 Support

All text processing correctly handles UTF-8:

```cpp
// Decode UTF-8 to code points for character iteration
std::vector<uint32_t> codepoints = utf8::decode(text);

// Compute glyph indices using Unicode code points
FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
```

The `charXOffsets` array is indexed by code point, not byte position.

### Special Case: List Item Position Propagation

List items set their children to relative positions during layout. Position propagation to absolute coordinates happens only at the document level to avoid double-application:

```cpp
bool skipPropagate = (childType == MarkdownObjectType::ListItem && !isRoot);
if (!skipPropagate) {
    propagatePositionToChildren(child, childX, childY);
}
```
