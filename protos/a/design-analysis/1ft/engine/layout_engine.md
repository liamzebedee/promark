# Design Analysis: LayoutEngine

**Files:** `src/engine/layout_engine.h`, `src/engine/layout_engine.cpp`

---

## 1. Responsibilities

The LayoutEngine has two declared responsibilities:

1. **Tree Transformation** (lines 18, 25-26 in .h): Convert a `MarkdownObject` tree into a parallel `LayoutObject` tree via `createLayoutTree()`.

2. **Layout Computation** (line 19 in .h): Compute positions and sizes for all layout objects via `performLayout()`.

Additionally, it holds font state:
- `fontFace` and `monoFontFace` (lines 22-23 in .h) - FreeType font handles that get injected into `TextLayoutObject` instances during tree creation.

---

## 2. Dependencies

### Direct Imports

| Import | Source | Purpose |
|--------|--------|---------|
| `markdown_objects.h` | .h:2 | Source tree types (`MarkdownObject`, `MarkdownObjectType`) |
| `layout_objects.h` | .h:3 | Target tree types (`LayoutObject`, all subclasses, `Rect`, `Size`, `LayoutFlow`) |
| `typography.h` | .cpp:2 | Layout constants (`DOCUMENT_MARGIN`, `BLOCK_SPACING`, `BLOCKQUOTE_INDENT`, `CODE_BLOCK_PADDING`) |
| `<ft2build.h>`, `FT_FREETYPE_H` | .h:4-5 | FreeType types (`FT_Face`) |

### Implicit Coupling

- **LayoutObject subclass constructors**: The engine directly instantiates 8 concrete layout types (lines 92-131 in .cpp):
  - `BlockLayoutObject`, `InlineLayoutObject`, `TextLayoutObject`, `ImageLayoutObject`
  - `TableLayoutObject`, `TableRowLayoutObject`, `TableCellLayoutObject`, `ListItemLayoutObject`

- **LayoutObject internal structure**: Accesses `getChildren()`, `setRect()`, `getRect()`, `getFlow()`, `isAtomic()`, `layout()`, `getSourceObject()` throughout.

---

## 3. Mutation Points

### State Owned

| Field | Mutated By | Authority |
|-------|------------|-----------|
| `fontFace` | `setFontFace()` (line 11-13 .cpp) | External caller |
| `monoFontFace` | `setMonoFontFace()` (line 15-17 .cpp) | External caller |

### State Mutated in Others

| Target | How | Location |
|--------|-----|----------|
| `TextLayoutObject::fontFace` | `setFontFace()` | .cpp:100, 103 |
| `TextLayoutObject::monoFontFace` | `setMonoFontFace()` | .cpp:106 |
| `TextLayoutObject::isMonospace` | `setMonospace(true)` | .cpp:101 |
| `LayoutObject::rect` (all nodes) | `setRect()` | .cpp:179, 192, 216, 225, 238 |

**Authority Issue**: The LayoutEngine injects fonts into TextLayoutObject during creation (lines 98-109), but the fonts are raw `FT_Face` pointers with no ownership semantics. If fonts are released externally, TextLayoutObject holds dangling pointers.

---

## 4. Boundary Violations

### Direct FreeType Dependency

The header exposes `FT_Face` in the public API (lines 13-16 in .h):
```cpp
void setFontFace(FT_Face face);
void setMonoFontFace(FT_Face face);
FT_Face getFontFace() const;
FT_Face getMonoFontFace() const;
```

This couples any consumer of LayoutEngine to FreeType. A higher-level abstraction (e.g., `FontHandle` or font ID) would allow swapping font backends without API changes.

### Typography Constants Access

The engine directly reads `Typography::*` constants throughout `layoutBlockFlow()` (lines 145-157, 206 in .cpp):
```cpp
float marginLeft = isRoot ? Typography::DOCUMENT_MARGIN : 0.0f;
marginLeft = Typography::BLOCKQUOTE_INDENT;
marginLeft = Typography::CODE_BLOCK_PADDING;
currentY += Typography::BLOCK_SPACING;
```

This hardcodes styling policy into the layout algorithm. The engine should receive spacing values from a style/theme object rather than importing a global namespace.

### Knowledge of Markdown Semantics

`layoutBlockFlow()` contains extensive type-switching on `MarkdownObjectType` (lines 137-158, 163-182, 197-201 in .cpp):
```cpp
bool isRoot = (type == MarkdownObjectType::Document);
bool isBlockQuote = (type == MarkdownObjectType::BlockQuote);
bool isCodeBlock = (type == MarkdownObjectType::CodeBlock);
```

And `performLayout()` special-cases certain types (lines 63-73):
```cpp
if (type == MarkdownObjectType::Table ||
    type == MarkdownObjectType::TableRow ||
    type == MarkdownObjectType::TableCell ||
    type == MarkdownObjectType::ListItem) {
    layoutRoot->layout(availableSpace);
    return;
}
```

This breaks separation: the layout engine must understand markdown semantics rather than operating purely on layout primitives.

---

## 5. Declared-but-Unrealised Design

### LayoutFlow Abstraction (Partially Implemented)

The `LayoutFlow` enum (layout_objects.h:25-28) declares two modes:
```cpp
enum class LayoutFlow {
    Block,
    Inline
};
```

However:

**Block flow** is implemented in `layoutBlockFlow()` (lines 135-217 in .cpp) with full vertical stacking, margin handling, and position propagation.

**Inline flow** is stubbed (lines 232-238 in .cpp):
```cpp
void LayoutEngine::layoutInlineFlow(LayoutObject* layoutObject, const Size& availableSpace) {
    // TODO: Implement inline flow layout
    // - Flow children left-to-right
    // - Break lines when necessary
    // - Handle baseline alignment

    layoutObject->layout(availableSpace);
}
```

The TODO comments describe the intended behavior but the implementation delegates entirely to `LayoutObject::layout()`. Inline layout is actually handled inside `TextLayoutObject::layout()` (per layout_objects.h), not by the engine.

**Workaround**: The asymmetry is compensated by having `TextLayoutObject` contain its own text shaping and wrapping logic (`shapeText()`, `wrapText()` declared at lines 126-127 of layout_objects.h). This inverts control - the layout object does its own layout rather than being positioned by the engine.

### Virtual layout() Method Bypass Pattern

`LayoutObject` declares a virtual `layout()` method (layout_objects.h:45):
```cpp
virtual void layout(const Size& availableSpace);
```

The engine's `performLayout()` uses an inconsistent dispatch pattern:
1. **Atomic objects** (line 57-60): Calls `layoutRoot->layout()` directly
2. **Table/ListItem** (lines 66-72): Calls `layoutRoot->layout()` directly
3. **Block flow** (line 77): Calls engine's `layoutBlockFlow()` which then recurses
4. **Inline flow** (line 79): Calls engine's `layoutInlineFlow()` which just calls `layout()`

Some layout objects fully manage their own children (Table*, ListItem*), while others (Block*) have their children laid out by the engine. This split authority means:
- You cannot understand an object's layout by reading its `layout()` method alone
- The engine must track which types are "self-managing"

### createLayoutObject Type Switch

The `createLayoutObject()` method (lines 83-133 in .cpp) is a large switch statement that must be extended for every new `MarkdownObjectType`. This is the factory pattern without the factory abstraction - no registration mechanism, no polymorphic creation.

The switch groups types by layout behavior:
- Lines 85-92: Block types -> `BlockLayoutObject`
- Lines 94-95: ListItem -> `ListItemLayoutObject`
- Lines 97-110: Text (with font injection)
- Lines 112-122: Atomic/table types -> specialized classes
- Lines 124-128: Inline styles -> `InlineLayoutObject`
- Lines 130-131: Default fallback

Each group implies a semantic category, but these categories are not named or reified in the type system.

### Empty Paragraph Special Case

Lines 167-182 in .cpp contain special handling for empty paragraphs:
```cpp
if (childSource->getType() == MarkdownObjectType::Paragraph) {
    // ... check if paragraph is empty ...
    if (isEmpty) {
        child->setRect(Rect(marginLeft, currentY, availableSpace.width - marginLeft * 2, 0));
        continue;
    }
}
```

The comment at line 166 reveals intent: "Skip empty paragraphs visually (they exist for cursor positioning but don't add space)". This is an editor concern leaking into layout - the layout engine knows about cursor positioning requirements.

### ListItem Propagation Skip

Lines 194-201 contain a workaround for double position propagation:
```cpp
// For ListItem children, only propagate at document level to avoid double-propagation
// (ListItem::layout sets children to relative positions, and we only want to convert
// to absolute once, not both at List level AND Document level)
bool skipPropagate = (childType == MarkdownObjectType::ListItem && !isRoot);
if (!skipPropagate) {
    propagatePositionToChildren(child.get(), childX, childY);
}
```

This documents that `ListItemLayoutObject::layout()` uses relative positioning while the engine uses absolute positioning, requiring coordination to avoid corrupting positions. The comment explicitly describes the asymmetry.

---

## Summary

The LayoutEngine attempts to be a coordinator between markdown structure and layout geometry, but:

1. **Leaky abstraction**: FreeType types in public API
2. **Hardcoded policy**: Typography constants imported directly
3. **Mixed authority**: Some layout objects self-manage, others are engine-managed
4. **Incomplete abstraction**: Inline flow is declared but not implemented
5. **Semantic leakage**: Markdown type knowledge throughout layout code
6. **Editor concerns**: Empty paragraph handling for cursor positioning

The pattern of "declared abstraction + workaround code" appears in:
- `LayoutFlow::Inline` (stubbed) + TextLayoutObject self-shaping
- `LayoutObject::layout()` virtual dispatch + engine type-switches
- Relative/absolute positioning + skipPropagate flag
