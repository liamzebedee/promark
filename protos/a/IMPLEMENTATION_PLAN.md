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

### P0-2: Remove inputBuffer char[] from Engine
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: P0-1

### P0-3: Create RenderBackend Abstraction
- **Status**: COMPLETED
- **Complexity**: XL
- **Dependencies**: None

### P0-4: Unify Layout Authority
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: None

### P0-5: Inline Formatting Tree Model
- **Status**: MOSTLY COMPLETE
- **Complexity**: XL
- **Dependencies**: None

---

## P1 - HIGH PRIORITY FOUNDATION WORK

### P1-1: Create Unified DrawRect with RectRole Enum
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: None

### P1-2: Convert Flat DisplayList to Hierarchical PaintTree
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: P0-3

### P1-3: Create FontProvider Abstraction
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: None

### P1-4: Implement Operation-Based Undo/Redo
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: P0-1

### P1-5: Pre-Compute Styles in LayoutObject
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: None

### P1-6: Upgrade DrawText to Use Pre-Shaped Glyph Data
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: P1-3

### P1-7: Upgrade DrawImage with Texture ID, Source Rect, Tint
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: None

### P1-8: Implement Viewport Culling in Rasterization
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: P0-3, P1-2

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
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: P0-5
- **Solution**: Implemented ThematicBreak node support:
  - Added ThematicBreak to MarkdownObjectType enum
  - Added ThematicBreakObject class
  - Implemented parsing in markdown_parser.cpp for ---, ***, ___ patterns
  - Added layout handling in layout_engine.cpp with fixed height
  - Added paintThematicBreak() in painter.cpp to render horizontal line
- **Files**: `src/engine/markdown_objects.h`, `src/engine/markdown_parser.cpp`, `src/engine/layout_engine.cpp`, `src/engine/painter.cpp`

### P2-3: Nested Inline Formatting
- **Status**: COMPLETED
- **Complexity**: L
- **Dependencies**: P0-5
- **Solution**: Nested formatting works correctly via `createInlineChildren()`. Example: `***bold italic***` creates Strong > Emphasis > Text hierarchy.
- **Files**: `src/engine/markdown_parser.cpp`

### P2-4: Implement layoutInlineFlow()
- **Status**: SUPERSEDED
- **Complexity**: L
- **Dependencies**: P0-4, P0-5
- **Note**: The stub method layoutInlineFlow() was deleted as part of P3-2. The method was never called - explicit Text handling in performLayout() ensures TextLayoutObject::layout() is called directly. This item is no longer applicable.

### P2-5: Add Box Model (Margin/Padding)
- **Status**: COMPLETED
- **Complexity**: M
- **Dependencies**: None
- **Solution**: Added box model infrastructure to LayoutObject:
  - Created EdgeInsets struct with top/right/bottom/left values and convenience methods (all(), symmetric(), horizontal(), vertical())
  - Added margin and padding member variables to LayoutObject (initialized to zero)
  - Added setMargin(), setPadding(), getMargin(), getPadding() accessors
  - Added getContentBox() (alias for getRect), getPaddingBox(), getMarginBox() methods
  - Infrastructure enables debug visualization of box model and future layout refactoring
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
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: P0-5
- **Solution**: Deleted InlineLayoutObject class (layout_objects.h:75-79, layout_objects.cpp:117-123). Replaced usages in layout_engine.cpp with base LayoutObject using LayoutFlow::Block. The InlineLayoutObject was dead code - its layout() method was empty.

### P3-2: Delete LayoutFlow::Inline Enum
- **Status**: COMPLETED
- **Complexity**: S
- **Dependencies**: P3-1
- **Solution**: Deleted LayoutFlow::Inline enum value. Added explicit Text handling in performLayout() to ensure TextLayoutObject::layout() is called directly. Deleted layoutInlineFlow() method from LayoutEngine (was a stub). All layout objects now use LayoutFlow::Block.

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

*Last updated: 2026-01-14 - P2-2 marked COMPLETED (ThematicBreak node implementation)*

## Change Summary (2026-01-14)

**Dead Code Cleanup Completed:**
- **P3-1**: Deleted InlineLayoutObject class - was unused dead code with empty layout() method
- **P3-2**: Deleted LayoutFlow::Inline enum and layoutInlineFlow() stub method - explicit Text handling now calls TextLayoutObject directly
- **P2-4**: Marked SUPERSEDED as layoutInlineFlow() no longer exists; work handled by direct Text handling in performLayout()

**Documentation Cleanup:**
- Removed detailed solution descriptions from completed P0 items (P0-1 through P0-5) to reduce file noise
- Removed detailed solution descriptions from completed P1 items (P1-1 through P1-8) to improve readability
- Kept brief status/complexity/dependency info for quick reference
