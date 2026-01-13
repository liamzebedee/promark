# Design Analysis: markdown_objects.h / markdown_objects.cpp

**Files:**
- `/home/liam/Documents/projects/promark/protos/a/src/engine/markdown_objects.h`
- `/home/liam/Documents/projects/promark/protos/a/src/engine/markdown_objects.cpp`

---

## 1. Responsibilities

This module defines the **Document Object Model (DOM)** for parsed markdown content. It serves as the intermediate representation between raw markdown text and the layout/rendering pipeline.

**Core responsibilities:**

1. **Type enumeration** (lines 6-25): Defines `MarkdownObjectType` with 17 distinct node types covering block elements (Document, Heading, Paragraph, CodeBlock, etc.) and inline semantics (Bold, Italic, Link, etc.)

2. **Tree structure** (lines 65-102): `MarkdownObject` base class provides:
   - Parent-child relationships via `children` vector of `unique_ptr`
   - Type identification via `getType()`
   - Text content storage via `text` member
   - Raw source position tracking (`rawStart`, `rawEnd`, `textOffset`)
   - Inline annotation ranges for links and styles

3. **Specialized node types** (lines 104-223): Subclasses that add type-specific data:
   - `HeadingObject`: level (1-6)
   - `ImageObject`: src, alt
   - `LinkObject`: url
   - `ListObject`/`ListItemObject`: ordered flag, marker type, indent level
   - `CodeBlockObject`: language, code content
   - `TableObject`/`TableRowObject`/`TableCellObject`: alignment, header flag

---

## 2. Dependencies

**Header includes** (lines 1-4):
- `<vector>` - child storage, link/style ranges
- `<string>` - text content, URLs, paths
- `<memory>` - unique_ptr ownership

**No external dependencies.** This module is a pure data structure with no rendering, parsing, or I/O logic. This is architecturally correct for a DOM layer.

**Consumed by:**
- `markdown_parser.h` - creates DOM tree from text
- `layout_engine.h` - creates layout tree from DOM
- `layout_objects.h` - stores `const MarkdownObject*` reference
- `glyph_atlas.h` - uses `TextStyle` enum for font styling
- `paint_operations.h` - uses `TextStyle` enum for draw operations

---

## 3. Mutation Points

### Mutable State:

| Member | Set by | Authority |
|--------|--------|-----------|
| `children` | `addChild()` (line 71) | Parser during construction |
| `text` | `setText()` (line 75) | Parser during construction |
| `rawStart`, `rawEnd` | `setRawRange()` (lines 78-80) | Parser during construction |
| `textOffset` | `setTextOffset()` (line 82) | Parser during construction |
| `linkRanges` | `addLinkRange()` (line 86) | Parser during construction |
| `styleRanges` | `addStyleRange()` (line 90) | Parser during construction |

### Observation:

All mutation methods are public and unrestricted. The DOM appears designed for **single-pass construction by the parser**, after which it should be treated as immutable by downstream consumers (layout engine, painter).

**Concern:** There is no enforcement of immutability after construction. `LayoutObject` holds a `const MarkdownObject*` (layout_objects.h:56), which provides runtime const-correctness, but the DOM itself exposes mutable methods.

### Recommended Authority Model:
- **Parser**: sole writer
- **LayoutEngine, Painter, GlyphAtlas**: readers only

---

## 4. Boundary Violations

**None identified.** This module is cleanly isolated:

- Does not include any rendering headers (OpenGL, GLFW)
- Does not include parsing logic (no md4c, no text_buffer.h)
- Does not include layout logic (no layout_objects.h)
- Does not include any platform headers

The direction of dependencies is correct:
```
markdown_objects.h  <--  markdown_parser.h
                    <--  layout_objects.h
                    <--  layout_engine.h
                    <--  paint_operations.h
                    <--  glyph_atlas.h
```

No reverse dependencies exist. This module sits at the foundation of the data flow.

---

## 5. Declared-but-Unrealised Design

### 5.1 Enum Types Without Corresponding Object Classes

**Lines 7-14 declare inline formatting types:**
```cpp
enum class MarkdownObjectType {
    ...
    Bold,
    Italic,
    Underline,
    Link,
    ...
};
```

**Reality:** There is no `BoldObject`, `ItalicObject`, or `UnderlineObject` class. Instead:
- Bold/italic are handled via `InlineStyleRange` (lines 59-63) attached to the parent object
- Links are handled via `InlineLinkRange` (lines 35-39) attached to the parent object

**Implication:** The enum suggests a tree-structured inline model (Bold node contains children), but the implementation uses a **flat span annotation model**. The enum values `Bold`, `Italic`, `Underline` are declared but never instantiated as objects.

**Evidence of workaround:** `TextLayoutObject::getLinkRanges()` and `getStyleRanges()` (layout_objects.h:113-116) retrieve these annotations from the source `MarkdownObject`, bypassing the child tree entirely.

### 5.2 TextStyle Duplication

**Lines 42-56 define `TextStyle` as a bitmask:**
```cpp
enum class TextStyle : uint8_t {
    Normal = 0,
    Bold = 1 << 0,
    Italic = 1 << 1,
    Code = 1 << 2,
    BoldItalic = Bold | Italic
};
```

This is **separate from** `MarkdownObjectType::Bold` and `MarkdownObjectType::Italic`. The bitmask model is used at the rendering layer (glyph_atlas.h:19, paint_operations.h:52), while the enum remains vestigial.

**Asymmetry:** `Code` exists only in `TextStyle`, not in `MarkdownObjectType`. Yet `CodeBlock` exists as a `MarkdownObjectType`. This suggests inline code (`backtick`) was added as a style flag workaround rather than a proper node type.

### 5.3 Link Representation Split

Links have two representations:
1. `LinkObject` class (lines 124-131) - a tree node with URL
2. `InlineLinkRange` struct (lines 35-39) - a span annotation

**Usage pattern:** The parser appears to use `InlineLinkRange` for inline links within paragraphs, while `LinkObject` exists but its relationship to the span model is unclear. This is a **parallel representation** that likely emerged from incremental development.

### 5.4 Inconsistent Content Storage

| Type | Content stored in |
|------|-------------------|
| Base `MarkdownObject` | `text` member (line 96) |
| `CodeBlockObject` | separate `code` member (line 182) |
| `FrontmatterObject` | separate `content` member (line 192) |

**Question:** Why do `CodeBlockObject` and `FrontmatterObject` have their own content members when the base class already provides `text`? This suggests the base `text` field was added later, or these subclasses predate the base implementation.

### 5.5 Table Alignment Redundancy

Column alignments are stored in two places:
1. `TableObject::columnAlignments` (line 203) - vector of alignments
2. `TableCellObject::alignment` (line 222) - per-cell alignment

The `TableObject` stores alignments for the whole table (from markdown separator row), then each `TableCellObject` stores its own copy. This is **denormalized state** that must be kept in sync.

---

## Summary of Architectural Concerns

| Issue | Severity | Lines | Notes |
|-------|----------|-------|-------|
| Unused enum values (Bold, Italic, Underline) | Medium | 11-13 | Vestigial types, never instantiated |
| Dual style representation (enum vs bitmask) | Low | 11-13, 42-48 | Parallel models, may cause confusion |
| Dual link representation (object vs range) | Medium | 124-131, 35-39 | Unclear when to use which |
| Inconsistent content storage pattern | Low | 96, 182, 192 | Could unify on base `text` |
| Table alignment denormalization | Low | 203, 222 | Sync burden between table and cells |
| No immutability enforcement | Medium | 71-91 | All setters public, no freeze mechanism |

The module is **structurally sound** but shows signs of **incremental design evolution** where newer inline span models (InlineLinkRange, InlineStyleRange) coexist with older tree-node models (LinkObject, Bold/Italic enums). A cleanup pass could remove unused enum values and unify the content storage pattern.
