# Promark Implementation Plan

> Generated from comprehensive codebase audit comparing `src/*` against `specs/*`

## Executive Summary

This plan addresses architectural violations and missing features identified through systematic comparison of the implementation against specifications. Items are prioritized by:
- **P0**: Blocking architectural issues that prevent correct implementation of other features
- **P1**: High-priority foundation work required for proper architecture
- **P2**: Medium-priority enhancements and feature completion
- **P3**: Low-priority cleanup and dead code removal

---

## P0 - BLOCKING ARCHITECTURAL ISSUES

These must be resolved first as they block correct implementation of other features.

### P0-1: Create TextModel as Single Source of Truth
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: None
- **Problem**: Three copies of document exist:
  1. `inputBuffer char[]` in Engine (10MB, authoritative) - `engine.h:52-54`
  2. `TextBuffer` in Engine (sync'd via setText)
  3. `TextBuffer` in MarkdownRenderer (another copy)
- **Solution**: TextBuffer is now the single source of truth for document content with version tracking (getVersion()) and dirty flag (isDirty(), markClean())
- **Spec Reference**: `specs/05-text-buffer.md`
- **Files**: `src/engine/engine.h`, `src/engine/engine.cpp`, `src/engine/text_buffer.h`

### P0-2: Remove inputBuffer char[] from Engine
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: P0-1
- **Problem**: Raw `char*` buffer is authoritative instead of TextModel
- **Solution**: Engine no longer has inputBuffer char[] - all text operations now go through TextBuffer. MarkdownRenderer accepts a const TextBuffer* pointer instead of owning a copy, using version tracking to detect changes.
- **Files**: `src/engine/engine.h:52`, `src/engine/engine.cpp`

### P0-3: Create RenderBackend Abstraction
- **Status**: NOT STARTED
- **Complexity**: XL
- **Dependencies**: None
- **Problem**: GL calls split across 4 components:
  - Rasterizer: `rasterizer.cpp:151-159, 190-196` (scissor, texture creation)
  - GlyphAtlas: `glyph_atlas.cpp:12, 19-32, 38, 104-107` (texture management)
  - BatchRenderer: `batch_renderer.cpp` (shaders, VBO, drawing)
  - Engine: `engine.cpp:46-50, 135-136, 142-143, 184` (blend, clear, scissor)
- **Spec Reference**: `specs/04-rasterization.md` - "Single GL Authority Rule"
- **Files**: `src/engine/rasterizer.cpp`, `src/engine/glyph_atlas.cpp`, `src/engine/batch_renderer.cpp`, `src/engine/engine.cpp`

### P0-4: Unify Layout Authority
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: None
- **Problem**: Split authority - 4 of 8 layout objects positioned themselves:
  - `TableLayoutObject::layout()` - lines 550-576
  - `TableRowLayoutObject::layout()` - lines 587-624
  - `TableCellLayoutObject::layout()` - lines 635-662
  - `ListItemLayoutObject::layout()` - lines 670-693
- **Solution**:
  1. Removed special-case delegation in `performLayout()` for Table/TableRow/TableCell/ListItem
  2. Added new methods to LayoutEngine: `layoutTable()`, `layoutTableRow()`, `layoutTableCell()`, `layoutListItem()`, `computeTableColumnWidths()`
  3. Updated `layoutBlockFlow()` to call `layoutListItem()` directly for ListItem children
  4. Removed the `skipPropagate` flag workaround (ready for cleanup in P3-3)
  5. Updated the layout objects to have no-op `layout()` methods (marked as bypassed, kept for backwards compatibility)

  The engine is now the sole authority for positioning. All 12 tests pass.
- **Spec Reference**: `specs/02-layout-system.md` - "Engine Coordinates, Objects Measure"
- **Files**: `src/engine/layout_objects.cpp`, `src/engine/layout_engine.cpp`

### P0-5: Convert Inline Formatting from ANNOTATION to TREE Model
- **Status**: NOT STARTED
- **Complexity**: XL
- **Dependencies**: None
- **Problem**:
  - Bold/Italic/Underline enum values exist (`markdown_objects.h:11-13`) but NEVER instantiated
  - `InlineStyleRange` annotation model used instead (`markdown_parser.cpp:59-63`)
  - `InlineLinkRange` for links instead of Link nodes
- **Current**: `Paragraph { text: "Hello world", styleRanges: [{start:6, end:11, style:Bold}] }`
- **Required**: `Paragraph { children: [Text("Hello "), Strong { Text("world") }] }`
- **Spec Reference**: `specs/01-document-model.md` - "Formatting is Structural"
- **Files**: `src/engine/markdown_parser.cpp`, `src/engine/markdown_objects.h`

---

## P1 - HIGH PRIORITY FOUNDATION WORK

### P1-1: Create Unified DrawRect with RectRole Enum
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: None
- **Problem**: 4 separate rect types exist:
  - `DrawRectOp` (`paint_operations.h:37-47`)
  - `DrawDebugBorderOp` (`paint_operations.h:98-108`)
  - `DrawSelectionRectOp` (`paint_operations.h:125-136`)
  - `DrawCaretOp` (`paint_operations.h:110-123`)
- **Spec Reference**: `specs/03-rendering-pipeline.md` - unified DrawRect with role field
- **Files**: `src/engine/paint_operations.h`

### P1-2: Convert Flat DisplayList to Hierarchical PaintTree
- **Status**: NOT STARTED
- **Complexity**: L
- **Dependencies**: P0-3
- **Problem**: `DisplayList` is `std::vector<unique_ptr<PaintOp>>` (`paint_operations.h:155`)
- **Required**: Tree of PaintArtifacts with bounds, clipRect, children for culling
- **Spec Reference**: `specs/03-rendering-pipeline.md`
- **Files**: `src/engine/paint_operations.h`, `src/engine/painter.cpp`

### P1-3: Create FontProvider Abstraction
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: None
- **Problem**: FT_Face leaked in 23+ locations across public APIs:
  - `engine.h:69-70`
  - `markdown_renderer.h:30-31`
  - `layout_engine.h:13-16, 22-23`
  - `layout_objects.h:107-108, 122-123`
  - `rasterizer.h:52-56, 60, 63`
  - `batch_renderer.h:36`
  - `glyph_atlas.h:36`
- **Spec Reference**: `specs/02-layout-system.md` - "No Platform Types in Public APIs"
- **Files**: All headers listed above

### P1-4: Implement Operation-Based Undo/Redo
- **Status**: NOT STARTED
- **Complexity**: L
- **Dependencies**: P0-1
- **Problem**:
  - `UndoState` only stores text + cursor (`engine.h:12-15`)
  - No operations recorded, just full text copies
  - No scroll position stored
  - No redo support
- **Spec Reference**: `specs/05-text-buffer.md` - operation-based undo
- **Files**: `src/engine/engine.h`, `src/engine/engine.cpp`

### P1-5: Pre-Compute Styles in LayoutObject
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: None
- **Problem**: Layout objects compute styles on-demand via `getFontSize()` which switches on source type (`layout_objects.cpp:48-63`)
- **Required**: Styles should be resolved and cached during layout phase
- **Spec Reference**: `specs/02-layout-system.md` - "fully resolved style information"
- **Files**: `src/engine/layout_objects.cpp`, `src/engine/layout_objects.h`

### P1-6: Upgrade DrawText to Use Pre-Shaped Glyph Data
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: P1-3
- **Problem**: `DrawTextOp` has raw string (`paint_operations.h:64`)
- **Required**: Reference to ShapedTextRun from layout with glyph IDs and positions
- **Spec Reference**: `specs/03-rendering-pipeline.md` - "glyph data (pre-shaped by layout)"
- **Files**: `src/engine/paint_operations.h`, `src/engine/painter.cpp`

### P1-7: Upgrade DrawImage with Texture ID, Source Rect, Tint
- **Status**: NOT STARTED
- **Complexity**: S
- **Dependencies**: None
- **Problem**: `DrawImageOp` only has `destRect` and `imagePath` (`paint_operations.h:78-81`)
- **Required**: texture ID, source rect (for atlasing), tint color
- **Files**: `src/engine/paint_operations.h`

### P1-8: Implement Viewport Culling in Rasterization
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: P0-3, P1-2
- **Problem**: Rasterizer processes flat list without visibility checking
- **Required**: Skip entire subtrees whose bounds don't intersect viewport
- **Spec Reference**: `specs/04-rasterization.md` - tree traversal with culling
- **Files**: `src/engine/rasterizer.cpp`

---

## P2 - MEDIUM PRIORITY ENHANCEMENTS

### P2-1: Implement Parser Stub Methods
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: None
- **Problem**: 6 stub methods return null/false (`markdown_parser.cpp:677-705`):
  - `parseBlock()` returns nullptr
  - `parseInline()` returns nullptr
  - `isHeading()` returns false
  - `isBlockQuote()` returns false
  - `isCodeBlock()` returns false
  - `isList()` returns false
- **Files**: `src/engine/markdown_parser.cpp`

### P2-2: Add Missing Node Types (LineBreak, ThematicBreak)
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: P0-5
- **Problem**: LineBreak and ThematicBreak nodes not implemented
- **Spec Reference**: `specs/01-document-model.md`
- **Files**: `src/engine/markdown_objects.h`, `src/engine/markdown_parser.cpp`

### P2-3: Implement Nested Inline Formatting
- **Status**: NOT STARTED
- **Complexity**: L
- **Dependencies**: P0-5
- **Problem**: Cannot handle `***bold italic***` correctly
- **Files**: `src/engine/markdown_parser.cpp`

### P2-4: Implement layoutInlineFlow()
- **Status**: NOT STARTED
- **Complexity**: L
- **Dependencies**: P0-4, P0-5
- **Problem**: Stub at `layout_engine.cpp:232-238` with TODO comment
- **Files**: `src/engine/layout_engine.cpp`

### P2-5: Add Box Model (Margin/Padding)
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: None
- **Problem**: No margin/padding support on layout objects
- **Spec Reference**: `specs/02-layout-system.md` - box model requirements
- **Files**: `src/engine/layout_objects.h`, `src/engine/layout_objects.cpp`

### P2-6: Pre-Load Images Before Layout
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: None
- **Problem**: `ImageLayoutObject::computeImageSize()` does I/O during layout (`layout_objects.cpp:467-525`)
- **Required**: ImageLoader service loads dimensions before layout
- **Spec Reference**: `specs/02-layout-system.md` - "No I/O in Layout"
- **Files**: `src/engine/layout_objects.cpp`

### P2-7: Hit-Testing Should Return Source Positions
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: P0-1
- **Problem**: Hit test implementation exists but doesn't follow spec
- **Spec Reference**: `specs/02-layout-system.md` - hit-testing algorithm
- **Files**: `src/engine/markdown_renderer.cpp:201-362`

### P2-8: Implement Intelligent Clipboard Syntax Closure
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: P0-5
- **Problem**: Naive string copy in `engine.cpp:886-895`, doesn't close markdown syntax
- **Example**: Copying "llo **wor" from "Hello **world**" should produce "llo **wor**"
- **Spec Reference**: `specs/01-document-model.md`, `specs/06-edge-cases.md`
- **Files**: `src/engine/engine.cpp`, `src/engine/clipboard.cpp`

### P2-9: Implement Immutable Snapshots with Versioning
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: P0-1, P1-4
- **Problem**: No snapshot or versioning support in TextBuffer
- **Spec Reference**: `specs/05-text-buffer.md`
- **Files**: `src/engine/text_buffer.h`, `src/engine/text_buffer.cpp`

### P2-10: Remove Shell's diskContent Shadow Copy
- **Status**: NOT STARTED
- **Complexity**: S
- **Dependencies**: P0-1
- **Problem**: `edit.cpp:13` has `diskContent` string with O(n) comparison every frame (`edit.cpp:18, 319`)
- **Required**: Use TextBuffer.isDirty() flag instead (dirty tracking is now handled by TextBuffer via isDirty() and markClean())
- **Files**: `src/edit.cpp`

---

## P3 - LOW PRIORITY / DEAD CODE CLEANUP

### P3-1: Delete InlineLayoutObject Class
- **Status**: NOT STARTED
- **Complexity**: S
- **Dependencies**: P0-5
- **Location**: `layout_objects.h:69-73`, `layout_objects.cpp:94-100`

### P3-2: Delete LayoutFlow::Inline Enum
- **Status**: NOT STARTED
- **Complexity**: S
- **Dependencies**: P3-1
- **Location**: `layout_objects.h:27`

### P3-3: Delete skipPropagate Flag
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: P0-4
- **Solution**: The skipPropagate flag was already deleted as part of P0-4 (commit 48e9700). The engine is now the sole coordinator for layout positioning with no special-case delegation needed.
- **Location**: Was at `layout_engine.cpp:198-199`, now removed

### P3-4: Delete SetClipOp/RestoreClipOp
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: None
- **Solution**: Removed all dead code related to SetClip/RestoreClip operations:
  - Deleted `SetClip` and `RestoreClip` enum values from `PaintOpType`
  - Deleted `SetClipOp` and `RestoreClipOp` class definitions and implementations
  - Deleted `executeSetClip()` and `executeRestoreClip()` methods from Rasterizer
  - Deleted unused `currentClip` and `hasClip` member variables from Rasterizer
- **Location**: Was at `paint_operations.h:83-96`, now removed

### P3-5: Consolidate DrawDebugBorderOp, DrawSelectionRectOp, DrawCaretOp
- **Status**: NOT STARTED
- **Complexity**: S
- **Dependencies**: P1-1
- **Action**: Merge into unified DrawRect with role field

### P3-6: Remove domToRaw/rawToDOM Functions
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: P0-1, P0-2
- **Location**: `markdown_renderer.cpp:154-182, 507-552` (~100 lines)

### P3-7: Extract Keyboard Handling from Engine
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: None
- **Problem**: 260+ lines of GLFW-specific keyboard handling (`engine.cpp:226-489`)

### P3-8: Reduce Engine God Class
- **Status**: NOT STARTED
- **Complexity**: L
- **Dependencies**: P0-1, P1-4, P3-7
- **Problem**: 40+ member variables mixing concerns

---

## Dependency Graph

```
P0-1 (TextModel) ─────┬──> P0-2 (Remove inputBuffer)
                      ├──> P1-4 (Operation-based undo)
                      ├──> P2-7 (Hit-testing)
                      ├──> P2-9 (Immutable snapshots)
                      ├──> P2-10 (Remove diskContent)
                      └──> P3-6 (Remove domToRaw/rawToDOM)

P0-3 (RenderBackend) ─┬──> P1-2 (PaintTree hierarchy)
                      └──> P1-8 (Viewport culling)

P0-4 (Unify layout) ──┬──> P2-4 (layoutInlineFlow)
                      └──> P3-3 (Delete skipPropagate)

P0-5 (Inline tree) ───┬──> P2-2 (LineBreak/ThematicBreak)
                      ├──> P2-3 (Nested inline formatting)
                      ├──> P2-8 (Intelligent clipboard)
                      ├──> P3-1 (Delete InlineLayoutObject) ──> P3-2 (Delete enum)
                      └──> P2-4 (layoutInlineFlow)

P1-1 (DrawRect) ──────> P3-5 (Consolidate rect ops)

P1-3 (FontProvider) ──> P1-6 (Pre-shaped glyph data)
```

---

## Parallelization Groups

These groups can be worked on concurrently:

**Group A - Rendering Foundation:**
- P0-3, P1-1, P1-7

**Group B - Text Model Foundation:**
- P0-1 → P0-2

**Group C - Layout Authority:**
- P0-4, P1-5, P2-5, P2-6

**Group D - Parser/Document Model:**
- P0-5, P2-1

**Group E - Independent Cleanup:**
- P1-3, P3-7

---

## Key Files Reference

| File | Primary Issues |
|------|----------------|
| `engine.h/cpp` | God class, inputBuffer, undo, GL calls, keyboard handling |
| `markdown_objects.h` | Unused Bold/Italic/Underline enums, annotation model |
| `markdown_parser.cpp` | 6 stub methods, annotation model for inline formatting |
| `layout_objects.cpp` | Split authority (4 objects self-position), I/O during layout |
| `layout_engine.cpp` | stub layoutInlineFlow() |
| `paint_operations.h` | 4 rect types, flat DisplayList |
| `painter.cpp` | Queries upstream for MarkdownObjectType |
| `rasterizer.cpp` | Direct GL calls, no culling |
| `glyph_atlas.cpp` | Standalone with GL calls (should be in backend) |
| `batch_renderer.cpp` | Partial backend, missing clip/resource management |
| `text_buffer.h/cpp` | Never-called insert/delete, no snapshots/versioning |
| `edit.cpp` | Shadow diskContent, O(n) comparison |

---

*Last updated: 2026-01-14*
