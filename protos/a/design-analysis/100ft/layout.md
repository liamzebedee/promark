# Layout System: Synthesis

**Source:** `layout_engine.h/cpp`, `layout_objects.h/cpp`

---

## Core Model

Two components collaborate on layout: **LayoutEngine** transforms MarkdownObject trees into LayoutObject trees and coordinates positioning; **LayoutObject** subclasses hold geometry and (sometimes) compute their own layout.

**Geometry primitives:** `Point`, `Size`, `Rect`, `LayoutFlow::{Block,Inline}`

**Subclass taxonomy:**
| Class | Flow | Self-manages children? |
|-------|------|------------------------|
| BlockLayoutObject | Block | No (engine manages) |
| InlineLayoutObject | Inline | Stub |
| TextLayoutObject | Inline | Yes (shapeText, wrapText) |
| ImageLayoutObject | Block | Yes (dimension extraction) |
| Table/Row/Cell | Block | Yes (layout() handles hierarchy) |
| ListItemLayoutObject | Block | Yes (indent + marker) |

---

## Repeated Patterns

### Type-switching on MarkdownObjectType
Both files switch on markdown semantics rather than layout primitives:
- **Engine:** `createLayoutObject()` maps 10+ markdown types to layout classes; `layoutBlockFlow()` checks Document/BlockQuote/CodeBlock for margins
- **Objects:** Table classes use `dynamic_cast` to navigate hierarchy; DOM length has markdown-specific special cases

### FreeType leakage
`FT_Face` appears in both public APIs:
- Engine: `setFontFace()`, `setMonoFontFace()`, `getFontFace()`, `getMonoFontFace()`
- Objects: `TextLayoutObject` stores and uses `FT_Face` directly

Both files couple consumers to FreeType. No abstraction layer exists.

### Typography constants
Both import `typography.h` for hardcoded styling:
- Engine: `DOCUMENT_MARGIN`, `BLOCK_SPACING`, `BLOCKQUOTE_INDENT`, `CODE_BLOCK_PADDING`
- Objects: `BASE_FONT_SIZE`, `LIST_INDENT`

Policy is embedded in algorithm, not passed as configuration.

---

## Cross-cutting Concerns

### Font injection lifecycle
**Flow:** Engine receives fonts -> creates TextLayoutObject -> injects fonts via setters
**Problem:** If `layout()` is called before fonts are set, fallback logic produces inconsistent results. Font ownership is unclear - raw `FT_Face` pointers may dangle.

### Rect authority
Both engine and objects mutate `rect`:
- Engine: `setRect()` during `layoutBlockFlow()` position propagation
- Objects: `setRect()` in their own `layout()` methods

No clear authority. You cannot understand an object's final position from either file alone.

### Relative vs absolute positioning
ListItemLayoutObject uses relative child positioning. Engine uses absolute. Coordination requires `skipPropagate` flag to prevent double-conversion. Comment in engine explicitly documents this asymmetry.

---

## Unstable Boundaries

### Who does layout?
| Scenario | Authority |
|----------|-----------|
| Block children | Engine's `layoutBlockFlow()` |
| Inline children | Engine's `layoutInlineFlow()` (stub, delegates to object) |
| Text content | TextLayoutObject's `shapeText()`/`wrapText()` |
| Table structure | Table*LayoutObject's `layout()` |
| List items | ListItemLayoutObject's `layout()` |
| Images | ImageLayoutObject's `computeImageSize()` |

The engine is supposed to coordinate, but 4 of 8 layout object types fully self-manage. The `layout()` virtual method exists but dispatch is inconsistent.

### Resource loading in layout layer
ImageLayoutObject performs:
- File system access (`stbi_info`)
- Base64 decoding (embedded utility function)
- PNG header parsing
- JPEG decompression init

This is asset loading, not geometry computation. The intended flow (MarkdownObjects -> Layout -> Render) has I/O embedded in the middle layer.

---

## Paper Abstractions

### LayoutFlow::Inline
**Declared:** Enum value, `InlineLayoutObject` class, engine has `layoutInlineFlow()` method.
**Actual:** `layoutInlineFlow()` is a stub with a TODO. `InlineLayoutObject::layout()` is empty. All inline layout happens inside `TextLayoutObject::shapeText()`/`wrapText()`.
**Workaround:** TextLayoutObject contains complete text shaping, glyph positioning, and line wrapping. It doesn't participate in flow; it IS the flow.

### GlyphRun struct
**Declared:** Struct with glyphIds, positions, width, height. Vector stored in TextLayoutObject. Getter returns it.
**Actual:** `shapeText()` clears the vector but never populates it. Glyph data goes to `charXOffsets` instead.
**Evidence:** Dead data structure.

### Base LayoutObject methods
- `layout()` - empty stub, every subclass overrides
- `computeIntrinsicSize()` - returns zero, only Text and Image override

Base class exists for tree structure, not behavioral polymorphism.

### Virtual dispatch pattern
Engine's `performLayout()` does:
```
if (atomic) -> call layout()
if (table/list) -> call layout()
if (block) -> engine's layoutBlockFlow() -> recurses, calls layout() on atomic children
if (inline) -> call layout()
```
The virtual method is there but engine type-switches to decide whether to use it.

---

## Editor Concerns in Layout

**Empty paragraph handling:** Engine special-cases empty paragraphs, giving them zero height but keeping them in tree. Comment: "they exist for cursor positioning but don't add space."

**DOM length:** TextLayoutObject returns 1 for empty strings because "empty lines still occupy 1 DOM position."

Layout layer knows about cursor model.

---

## Summary: Structural Issues

1. **Split authority** - Engine and objects both mutate geometry, with undocumented rules about who wins
2. **Declared-not-implemented** - LayoutFlow::Inline, GlyphRun, base class methods exist as types without behavior
3. **Layer violations** - FreeType in public API, image I/O in layout, base64 in layout, editor concerns in layout
4. **Two positioning systems** - Relative (ListItem) vs absolute (everything else), coordinated by flag
5. **Type-switching as extension** - New markdown types require changes to both files' switch statements
6. **Hidden state mutation** - Mutable lazy fields hide I/O in what looks like getters
