# Promark Architecture: System-Level Summary

This document synthesizes architectural patterns across all subsystems. It focuses on system-wide structure, not module-specific details.

---

## 1. System-Level Responsibilities

### Intended Subsystem Ownership

| Subsystem | Should Own | Actually Owns |
|-----------|------------|---------------|
| **Shell** (main.cpp, edit.cpp) | Window management, input forwarding | + Image processing, markdown generation, dirty tracking, coordinate scaling |
| **Engine** | Input handling, editing coordination | + Raw buffer mutation, text shaping, raw-mode rendering, platform calls |
| **Document Model** (parser, objects) | Markdown syntax, AST structure | Only construction; no freeze/immutability enforcement |
| **Layout** | Geometry computation, positioning | + Image I/O, base64 decoding, font management |
| **Rendering** (renderer, painter) | Visual output coordination | + Query services (hit-testing, position mapping), semantic-to-style derivation |
| **Rasterization** | GPU operations, resource management | + Image loading/decoding, text shaping (UTF-8 decode, glyph positioning) |
| **Utilities** | Shared primitives | Paper APIs that higher layers bypass entirely |

### Responsibility Violations Summary

Three patterns of responsibility drift:

1. **Upward leakage**: Low-level concerns appear in high-level modules (GLFW constants in Engine, FreeType in Layout API)
2. **Downward leakage**: Domain knowledge pushes into infrastructure (markdown syntax generation in Shell, semantic type queries in Painter)
3. **Sideways duplication**: Parallel implementations (dirty tracking in both Engine and Shell, two GlyphAtlas instances, raw mode as shadow renderer)

---

## 2. Single Sources of Truth

### Where Authority Should Live vs. Where It Actually Lives

| Concern | Intended Authority | Actual State |
|---------|-------------------|--------------|
| **Document content** | TextBuffer | Three copies: `char[10MB]` (Engine), `TextBuffer` (Engine), `TextBuffer` (MarkdownRenderer). The raw char array is authoritative. |
| **Cursor position** | Engine | Split: position in Engine, animation in CaretState, geometry in Painter |
| **Dirty state** | Engine (`isDirty()`) | Duplicated: Engine tracks via flags, Shell tracks via string comparison |
| **Inline formatting** | MarkdownObject tree | Three parallel models: tree nodes (declared, unused), span annotations (runtime), bitmask enum (rendering) |
| **Table alignment** | TableObject | Denormalized into both parent table and child cells |
| **Font state** | Rasterizer | Split: FT_Face threaded through Layout, Rasterizer, BatchRenderer, GlyphAtlas |
| **GL state** | BatchRenderer | Split across Rasterizer (scissor, textures), BatchRenderer (blend, programs), GlyphAtlas (unpack alignment) |

### Critical Split: Position Space

Two coordinate systems exist throughout:
- **Raw positions**: Byte offsets into original markdown text
- **DOM positions**: Character offsets in rendered/parsed content

Translation happens via `rawToDOM()`/`domToRaw()` in MarkdownRenderer, but:
- Engine thinks in raw positions (cursor, selection)
- Layout stores DOM-relative offsets
- Hit-testing returns raw positions
- Every frame requires bidirectional translation

This is the most pervasive authority split. The architecture never committed to one position space.

---

## 3. State Ownership and Write Paths

### Mutable State Map

```
Shell Layer
  |-- window dimensions, scale factors (computed, discarded)
  |-- diskContent cache (edit.cpp only, parallel to Engine.isDirty)

Engine (God Class - 40+ mutable fields)
  |-- char inputBuffer[10MB]     <-- TRUE AUTHORITY for content
  |-- TextBuffer* textBuffer     <-- sync'd via full copy
  |-- cursor/selection positions
  |-- undo stack (single level)
  |-- showRaw toggle
  |-- scroll state
  |-- UI state (toolbar, hover)

MarkdownRenderer
  |-- TextBuffer (copy from Engine)
  |-- objectTree (rebuilt on reparse)
  |-- layoutTree (rebuilt on relayout)
  |-- dirty flags (reparse/relayout/repaint)
  |-- cached query results

LayoutEngine
  |-- font faces (passed through, not owned)
  |-- width constraints

Rasterizer
  |-- FT_Library, FT_Face instances (OWNS font resources)
  |-- image texture cache
  |-- clip stack

GlyphAtlas
  |-- texture, glyph cache (per-instance, not shared)
```

### Write Path for Text Mutation

Every edit (keystroke, paste, formatting) follows:

```
1. Engine::memmove/memcpy on inputBuffer
2. Engine::textBuffer->setText(inputBuffer)           // Full copy #1
3. Engine::markdownRenderer->setTextBuffer(copy)      // Full copy #2
4. MarkdownRenderer::needsReparse = true
5. Next render(): MarkdownParser::parse()             // Rebuild tree
6. Next render(): LayoutEngine::layout()              // Rebuild geometry
7. Next render(): Painter::paint()                    // Rebuild display list
8. Rasterizer::rasterize()                            // GPU upload
```

Steps 2-3 could be eliminated if TextBuffer were authoritative. Currently, every mutation incurs O(n) copy overhead twice.

---

## 4. Dependency Direction

### Intended Layering

```
       Shell
         |
       Engine
         |
  +------+------+
  |             |
Document    MarkdownRenderer
  Model          |
            +----+----+
            |         |
         Layout    Painter
            |         |
            +----+----+
                 |
            Rasterizer
                 |
           BatchRenderer
                 |
            GlyphAtlas
```

### Actual Dependency Inversions

| Violation | Nature |
|-----------|--------|
| Shell -> Engine internals | Shell calls `insertText()`, `getContent()` directly, bypassing input API |
| Engine -> GLFW | 260+ lines of GLFW constants embedded in input handling |
| Engine -> OpenGL | Direct GL calls in initialize() and render() |
| Layout -> FreeType | FT_Face in public API, layout objects store raw pointers |
| Layout -> stb_image | Image I/O embedded in ImageLayoutObject |
| Painter -> markdown_objects | Queries MarkdownObjectType to derive visual style |
| paint_operations -> markdown_objects | TextStyle enum dependency |
| Rasterizer -> markdown_objects | TextStyle enum dependency |
| BatchRenderer -> UTF-8 | Text shaping logic (decode, position) in rendering layer |
| GlyphAtlas -> FT_Face | Mutates external face's pixel size |

### The FreeType Thread

FreeType dependencies weave through every layer:
```
Engine -> MarkdownRenderer -> LayoutEngine -> TextLayoutObject
                                   |
                            Rasterizer -> BatchRenderer -> GlyphAtlas
```

No abstraction exists. Replacing FreeType would require changes to 8+ files across 5 layers.

---

## 5. Architectural Drift

### Document Model

**Intended**: Recursive descent parser with modular block/inline detection. Tree-structured DOM with formatting as child nodes.

**Actual**: 516-line monolithic `parseDocument()` with inline detection. Inline formatting via span annotations on parent nodes, bypassing tree structure. Six stubbed detection methods (`parseBlock`, `parseInline`, `isHeading`, `isBlockQuote`, `isCodeBlock`, `isList`) return false/nullptr.

### Layout System

**Intended**: Engine coordinates layout; LayoutObjects compute intrinsic sizes. Clean flow dispatch (Block vs Inline).

**Actual**: Four of eight layout object types fully self-manage (`layout()` does everything). `layoutInlineFlow()` is a stub - TextLayoutObject contains complete text shaping internally. Two positioning systems (relative for ListItem, absolute for everything else) coordinated by `skipPropagate` flag.

### Rendering Pipeline

**Intended**: Parse -> Layout -> Paint -> Rasterize with display list as intermediate representation.

**Actual**: Display list exists but provides no value - Rasterizer switch-statement is the real protocol. MarkdownRenderer accumulates 350+ lines of query logic that directly traverses both trees. The "coordinator" became an ad-hoc query service.

### Rasterization

**Intended**: BatchRenderer as sole GL abstraction. PaintOps as command protocol.

**Actual**: Split GL authority across three components (Rasterizer, BatchRenderer, GlyphAtlas). PaintOp inheritance used only as type marker - dispatch is explicit switch on enum. Image rendering bypasses batching entirely.

### TextBuffer

**Intended**: Central text model with position-based editing API (`insertText`, `deleteText`).

**Actual**: Never called. Engine uses `char[10MB]` with `memmove`/`memcpy`, then syncs via `setText()`. TextBuffer is a serialization format, not an editing API.

---

## 6. Largest Unrealised/Partial Abstractions

### 1. TextBuffer as Editing API

**Weight**: Present in Engine, MarkdownRenderer, and utilities layer.
**Reality**: API exists (`insertText`, `deleteText`) but is never called. Dual-write pattern on every mutation (raw buffer + copy sync) adds complexity without abstraction benefit.
**Forcing**: Every editing operation requires two updates. Silent divergence risk if either is forgotten.

### 2. LayoutFlow::Inline

**Weight**: Enum value, `InlineLayoutObject` class, `layoutInlineFlow()` method in engine.
**Reality**: All inline layout happens inside `TextLayoutObject::shapeText()`/`wrapText()`. The "flow" abstraction is structural only.
**Forcing**: Text layout cannot participate in flow - it IS the flow. Composing inline elements (bold + link) requires span annotations, not tree composition.

### 3. PaintOp Display List

**Weight**: 9 operation types, inheritance hierarchy, dedicated header.
**Reality**: No polymorphic dispatch. Rasterizer uses explicit type-switch. Four structurally-identical rect operations could be one type with role enum.
**Forcing**: Adding new paint operations requires: define class, add enum value, add switch case. Three places instead of one.

### 4. Platform Abstraction (Clipboard, GL, Coordinate Scaling)

**Weight**: Clipboard has callback injection. gl_includes.h has platform branches. Shell computes scale factors.
**Reality**: GLFW hard-included despite "abstraction". GL platform logic incomplete (no Windows). Scale factors computed and discarded - Engine cannot access them.
**Forcing**: Web/mobile ports would require stubbing GLFW headers, fixing GL includes, and threading scale factors through API boundaries.

### 5. Engine's Lifecycle API

**Weight**: `isDirty()`, `markClean()`, `shouldClose()` methods on Engine.
**Reality**: Completely bypassed by Shell. edit.cpp implements parallel dirty tracking via string comparison. main.cpp polls `shouldClose()` but also has its own close handling.
**Forcing**: Two competing dirty/lifecycle systems that can diverge. No event callbacks from Engine to Shell.

### 6. Raw Mode as View Toggle

**Weight**: `showRaw` boolean, appears to be simple mode switch.
**Reality**: 230 lines of parallel rendering (text wrapping, hit testing, cursor positioning, selection, syntax highlighting) with different assumptions (monospace only, hardcoded sizes, different colors).
**Forcing**: Two rendering pipelines that must be kept in sync. Features added to one must be reimplemented in the other.

---

## Summary: The Core Tensions

1. **Authority vs. Abstraction**: Engine owns a raw `char[10MB]` buffer but also owns a TextBuffer it bypasses. The abstraction exists but isn't authoritative.

2. **Coordination vs. Implementation**: Engine and MarkdownRenderer are nominally coordinators that accumulated implementation. Engine does raw text manipulation; Renderer does query logic.

3. **Layering vs. Pragmatism**: Clean layer boundaries were drawn (Shell/Engine/Layout/Render/Raster) but practical needs embedded platform dependencies and cross-layer queries throughout.

4. **Declared vs. Implemented**: Multiple features exist structurally (interfaces, stubs, types) without mechanical implementation. These "paper abstractions" add complexity without capability.

5. **Single vs. Dual**: Position spaces (raw/DOM), dirty tracking (Engine/Shell), formatting models (tree/spans/enum), GL authority (Rasterizer/BatchRenderer/Atlas). System-wide, the architecture frequently represents concepts twice.

The architecture reveals a system that intended clean separation but accumulated pragmatic shortcuts. The shortcuts became load-bearing. Remediation requires either committing to the abstractions (making TextBuffer authoritative, implementing inline flow, completing display list protocol) or removing them (acknowledging Engine's raw buffer as source of truth, collapsing layout into TextLayoutObject, switching PaintOp to variant).
