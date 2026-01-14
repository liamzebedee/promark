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
- **Status**: COMPLETED
- **Complexity**: XL
- **Dependencies**: None
- **Solution**: Created unified RenderBackend architecture:
  - Created `RenderBackend` abstract interface in `render_backend.h` with:
    - Frame lifecycle: `init()`, `beginFrame()`, `endFrame()`
    - Viewport/clear: `setViewport()`, `clear()`
    - Clipping: `pushClip()`, `popClip()`
    - Drawing: `drawRect()`, `drawLine()`, `drawText()`, `drawImage()`
    - Textures: `createTexture()`, `deleteTexture()`
    - Batching: `setScrollOffset()`, `flush()`
  - Created `OpenGLBackend` implementation absorbing all GL calls from:
    - GlyphAtlas (glyph texture management)
    - BatchRenderer (shaders, VBO, drawing)
    - Engine (viewport, clear, scissor, blend)
    - Rasterizer (image texture creation)
  - Updated `Rasterizer` to use `RenderBackend*` interface
  - Updated `MarkdownRenderer` with `setBackend()` method
  - Updated `Engine` to create/own `OpenGLBackend` and pass to subsystems
  - Removed `glyph_atlas.cpp` and `batch_renderer.cpp` from build (absorbed into backend)
  - All 12 tests pass
- **Spec Reference**: `specs/04-rasterization.md` - "Single GL Authority Rule"
- **Files**: `src/engine/render_backend.h`, `src/engine/opengl_backend.h`, `src/engine/opengl_backend.cpp`, `src/engine/rasterizer.h`, `src/engine/rasterizer.cpp`, `src/engine/markdown_renderer.h`, `src/engine/markdown_renderer.cpp`, `src/engine/engine.h`, `src/engine/engine.cpp`, `sources.mk`

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

### P0-5: Inline Formatting Tree Model
- **Status**: MOSTLY COMPLETE
- **Complexity**: XL
- **Dependencies**: None
- **Current State**:
  - Tree structure is fully implemented via `createInlineChildren()` method (`markdown_parser.cpp:262-453`)
  - Structural tree nodes are created and used: `StrongObject`, `EmphasisObject`, `InlineCodeObject`, `LineBreakObject`, `StrikethroughObject`
  - Nested formatting works correctly (e.g., `***bold italic***` creates Strong > Emphasis > Text)
  - Annotations (`InlineStyleRange`) are DERIVED from the tree via `buildStyleRangesFromTree()` for layout/paint compatibility
- **Architecture**: The parser creates hierarchical inline nodes per spec. Style ranges are derived from the tree for the current layout/paint implementation.
- **Remaining Work**: Optionally migrate layout/paint to consume the tree directly (may not be necessary given the current approach works correctly)
- **Spec Reference**: `specs/01-document-model.md` - "Formatting is Structural"
- **Files**: `src/engine/markdown_parser.cpp`, `src/engine/markdown_objects.h`, `src/engine/markdown_objects.cpp`

---

## P1 - HIGH PRIORITY FOUNDATION WORK

### P1-1: Create Unified DrawRect with RectRole Enum
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: None
- **Solution**: Created unified DrawRect with RectRole enum:
  - Added `RectRole` enum with values: Background, Selection, Caret, Border, Debug
  - Extended `DrawRectOp` with role field and getRole() accessor
  - Added convenience constructor (defaults to Background role) for backwards compatibility
  - Removed separate `PaintOpType` enum values for DrawDebugBorder, DrawCaret, DrawSelectionRect
  - Deleted `DrawDebugBorderOp`, `DrawCaretOp`, `DrawSelectionRectOp` classes
  - Updated painter.cpp to use DrawRectOp with appropriate roles
  - Updated rasterizer to handle all roles in unified executeDrawRect():
    - Background/Selection: filled rectangle
    - Caret: filled rectangle (respects visibility for blinking)
    - Border: 1px stroke (4 thin rectangles)
    - Debug: 2px stroke for layout visualization
  - All 12 tests pass
- **Spec Reference**: `specs/03-rendering-pipeline.md` - unified DrawRect with role field
- **Files**: `src/engine/paint_operations.h`, `src/engine/paint_operations.cpp`, `src/engine/painter.cpp`, `src/engine/rasterizer.h`, `src/engine/rasterizer.cpp`

### P1-2: Convert Flat DisplayList to Hierarchical PaintTree
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: P0-3
- **Solution**: Implemented hierarchical PaintTree structure for efficient viewport culling:
  - Created `PaintArtifact` class in `paint_operations.h` with:
    - `displayItems`: Drawing commands for this node
    - `bounds`: Bounding box for viewport culling
    - `clipRect`: Optional structural clip region (not push/pop)
    - `children`: Child artifacts (owned via unique_ptr)
  - Added `PaintTree` type alias (`std::unique_ptr<PaintArtifact>`)
  - Updated `Painter::paint()` to return `PaintTree` instead of `DisplayList`
  - Updated `Painter::paintLayoutObject()` to create hierarchical artifacts mirroring layout tree
  - Updated `Rasterizer::rasterize()` to accept `PaintTree` with viewport culling:
    - `rasterizeArtifact()` recursively traverses tree
    - `boundsIntersectViewport()` enables subtree culling
    - Entire subtrees outside viewport are skipped
  - Added `rasterizeDisplayList()` for legacy compatibility
  - Updated `MarkdownRenderer` to use `PaintTree` (renamed `displayList` to `paintTree`)
  - All 12 tests pass
- **Benefits**:
  - Culling: Skip entire subtrees outside viewport
  - Clipping: Structural property, not stateful push/pop
  - Caching: Unchanged subtrees can be reused (future)
  - Debugging: Paint tree mirrors layout tree structure
- **Spec Reference**: `specs/03-rendering-pipeline.md`
- **Files**: `src/engine/paint_operations.h`, `src/engine/paint_operations.cpp`, `src/engine/painter.h`, `src/engine/painter.cpp`, `src/engine/rasterizer.h`, `src/engine/rasterizer.cpp`, `src/engine/markdown_renderer.h`, `src/engine/markdown_renderer.cpp`

### P1-3: Create FontProvider Abstraction
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: None
- **Solution**: Created FontProvider abstraction to remove FT_Face from layout layer public APIs:
  - Created `FontProvider` abstract interface in `font_provider.h` with:
    - `getGlyphAdvance(codepoint, fontSize, monospace)` for glyph measurement
    - `getLineHeight(fontSize, monospace)` for line metrics
    - `getFallbackCharWidth(fontSize, monospace)` with default implementation
  - Created `FreeTypeFontProvider` concrete implementation that wraps FT_Face
  - Updated `LayoutEngine` to use `FontProvider*` instead of `FT_Face`
  - Updated `TextLayoutObject` to use `FontProvider*` for text measurement
  - Updated `MarkdownRenderer` to accept `FontProvider*` via `setFontProvider()`
  - Updated `Engine` to create and own `FreeTypeFontProvider`
  - Note: `FT_Face` remains in rendering layer (Rasterizer, BatchRenderer, GlyphAtlas) which is appropriate since they're internal to the rendering backend
  - All 12 tests pass
- **Spec Reference**: `specs/02-layout-system.md` - "No Platform Types in Public APIs"
- **Files**: `src/engine/font_provider.h`, `src/engine/freetype_font_provider.h`, `src/engine/freetype_font_provider.cpp`, `src/engine/layout_engine.h`, `src/engine/layout_objects.h`, `src/engine/markdown_renderer.h`, `src/engine/engine.h`

### P1-4: Implement Operation-Based Undo/Redo
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: P0-1
- **Solution**: Implemented full operation-based undo/redo system:
  - Created `TextOpType` enum (Insert, Delete, Replace)
  - Created `TextOperation` struct with position, insertedText, deletedText
  - Created `UndoEntry` struct with operation, caretPositionBefore, scrollPositionBefore
  - Added `redoStack` and `redo()` method
  - Implemented `recordInsert()`, `recordDelete()`, `recordReplace()` to record operations
  - Updated all text mutation sites (17 locations) to use operation recording
  - Added keyboard shortcuts: Ctrl+Shift+Z and Ctrl/Cmd+Y for redo
  - Undo now restores both caret and scroll position
  - All 12 tests pass
- **Spec Reference**: `specs/05-text-buffer.md` - operation-based undo
- **Files**: `src/engine/engine.h`, `src/engine/engine.cpp`

### P1-5: Pre-Compute Styles in LayoutObject
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: None
- **Solution**: Added font size caching and character-level style pre-computation:
  - Added font size caching to LayoutObject with automatic invalidation
  - Added ComputedCharStyles struct for character-level style caching
  - TextLayoutObject::computeCharStyles() pre-computes styles during layout
  - Painter now uses pre-computed styles instead of recomputing every paint
  - Performance improvements: Font size lookups O(1), style arrays computed once per layout
  - All 12 tests pass
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
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: None
- **Solution**: Extended DrawImageOp with pre-loaded texture support:
  - Added `textureId` field (0 = load from path, non-zero = pre-loaded)
  - Added `sourceRect` field for texture atlasing (normalized 0-1 coordinates)
  - Added `tintColor` field for image colorization/effects
  - Updated BatchRenderer::drawImage() to use source rect and tint
  - Rasterizer uses textureId if provided, falls back to path-based loading
  - Backwards compatible: convenience constructor defaults to full rect, white tint
- **Files**: `src/engine/paint_operations.h`, `src/engine/paint_operations.cpp`, `src/engine/rasterizer.cpp`, `src/engine/batch_renderer.h`, `src/engine/batch_renderer.cpp`

### P1-8: Implement Viewport Culling in Rasterization
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: P0-3, P1-2
- **Solution**: Viewport culling was implemented as part of P1-2 (hierarchical PaintTree):
  - `rasterize()` method accepts `PaintTree` and `viewport`, calling `rasterizeArtifact()` for traversal
  - `rasterizeArtifact()` recursively processes artifacts with `boundsIntersectViewport()` check
  - `boundsIntersectViewport()` implements proper AABB intersection test to skip subtrees outside viewport
  - Entire subtrees are culled when bounds don't intersect viewport, improving performance
- **Spec Reference**: `specs/04-rasterization.md` - tree traversal with culling
- **Files**: `src/engine/rasterizer.cpp` (rasterizeArtifact(), boundsIntersectViewport())

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

### P2-2: Add Missing Node Types (ThematicBreak)
- **Status**: NOT STARTED
- **Complexity**: M
- **Dependencies**: P0-5
- **Problem**: ThematicBreak node not implemented (LineBreak is already implemented via `LineBreakObject`)
- **Spec Reference**: `specs/01-document-model.md`
- **Files**: `src/engine/markdown_objects.h`, `src/engine/markdown_parser.cpp`

### P2-3: Nested Inline Formatting
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: P0-5
- **Solution**: Nested formatting works correctly via `createInlineChildren()`. Example: `***bold italic***` creates Strong > Emphasis > Text hierarchy.
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
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: P0-1
- **Solution**: Replaced O(n) string comparison with TextBuffer's dirty flag:
  - Removed `diskContent` global variable that duplicated entire file content
  - Updated `isDirty()` to use `engine->isDirty()` instead of string comparison
  - Updated `saveFile()` to call `engine->markClean()` instead of updating shadow copy
  - Removed redundant initialization since `engine->setContent()` already marks buffer as clean
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
- **Solution**: Deleted as part of P0-4. Engine is now sole coordinator for layout positioning with no special-case delegation.

### P3-4: Delete SetClipOp/RestoreClipOp
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: None
- **Solution**: Removed dead SetClip/RestoreClip operations, enum values, class definitions, and associated Rasterizer methods (currentClip, hasClip members).

### P3-5: Consolidate DrawDebugBorderOp, DrawSelectionRectOp, DrawCaretOp
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: P1-1
- **Solution**: Merged into unified DrawRect with role field as part of P1-1. Separate classes deleted; all usages use DrawRectOp with appropriate RectRole.

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

P0-3 (RenderBackend) ─┬──> P1-2 (PaintTree hierarchy) [BOTH COMPLETE]
                      └──> P1-8 (Viewport culling) [COMPLETE - implemented via P1-2]

P0-4 (Unify layout) ──┬──> P2-4 (layoutInlineFlow)
                      └──> P3-3 (Delete skipPropagate)

P0-5 (Inline tree) ───┬──> P2-2 (LineBreak/ThematicBreak)
                      ├──> P2-3 (Nested inline formatting)
                      ├──> P2-8 (Intelligent clipboard)
                      ├──> P3-1 (Delete InlineLayoutObject) ──> P3-2 (Delete enum)
                      └──> P2-4 (layoutInlineFlow)

P1-1 (DrawRect) ──────> P3-5 (Consolidate rect ops) [BOTH COMPLETE]

P1-3 (FontProvider) ──> P1-6 (Pre-shaped glyph data) [P1-3 COMPLETE]
```

---

## Parallelization Groups

These groups can be worked on concurrently:

**Group A - Rendering Foundation:**
- ~~P0-3~~, ~~P1-1~~, ~~P1-2~~, ~~P1-7~~ (all complete)

**Group B - Text Model Foundation:**
- ~~P0-1~~ → ~~P0-2~~ (both complete)

**Group C - Layout Authority:**
- ~~P0-4~~, P1-5, P2-5, P2-6 (P0-4 complete)

**Group D - Parser/Document Model:**
- P0-5 (mostly complete), P2-1, ~~P2-3~~ (complete)

**Group E - Independent Cleanup:**
- ~~P1-3~~, P3-7 (P1-3 complete)

---

## Key Files Reference

| File | Primary Issues |
|------|----------------|
| `engine.h/cpp` | God class, keyboard handling (GL calls moved to backend) |
| `markdown_objects.h` | Tree model implemented (Strong, Emphasis, etc.) |
| `markdown_parser.cpp` | 6 stub methods, tree model with derived annotations for layout/paint |
| `layout_objects.cpp` | I/O during layout (split authority resolved) |
| `layout_engine.cpp` | stub layoutInlineFlow() |
| `paint_operations.h` | PaintTree hierarchical (PaintArtifact with bounds, clipRect, children) |
| `painter.cpp` | Produces hierarchical PaintTree (queries upstream for MarkdownObjectType) |
| `rasterizer.cpp` | Viewport culling implemented (GL calls through backend) |
| `opengl_backend.cpp` | Single GL authority - all rendering goes through here |
| `text_buffer.h/cpp` | Never-called insert/delete, no snapshots/versioning |
| `edit.cpp` | - |

---

*Last updated: 2026-01-14 - P1-5 marked COMPLETED (font size caching and character-level style pre-computation)*
