# Refactor Plan: Promark Architecture Simplification

This plan identifies the smallest set of refactors that maximally simplify the design, ordered by impact and risk. Each refactor targets either:
- **Collapsing unrealised abstractions** (removing paper abstractions that add complexity without capability)
- **Deleting workaround code** (making underlying mechanisms real OR removing the pretense)
- **Enforcing clean dependencies** (one-way dependencies, single-writer state)

---

## Impact/Risk Matrix

| Priority | Impact | Risk | Category |
|----------|--------|------|----------|
| P0 | High | Low | Remove dead abstractions |
| P1 | High | Medium | Consolidate split authority |
| P2 | Medium | Medium | Extract shared types |
| P3 | Medium | High | Platform abstraction |
| P4 | Low | Low | Dead code cleanup |

---

## P0: High Impact, Low Risk

### R1. Delete TextBuffer Editing API (Remove Pretense)

**Problem:** `TextBuffer::insertText()` and `deleteText()` are implemented but never called. Engine uses raw `char[10MB]` buffer with `memmove`/`memcpy`, then syncs via `setText()`.

**Current complexity:**
- 18+ dual-write sites in engine.cpp
- Three copies of document state (inputBuffer, Engine::textBuffer, MarkdownRenderer::textBuffer)
- Every keystroke triggers O(n) copy

**Refactor:**
1. Delete `insertText()` and `deleteText()` from TextBuffer
2. Rename TextBuffer to `TextSnapshot` (what it actually is)
3. Document that Engine::inputBuffer is authoritative

**Impact:** Removes ~50 lines of unused code, clarifies true authority
**Risk:** Low - deleting code that's never called
**Workaround deleted:** The fiction that TextBuffer is an editing API

---

### R2. Delete Parser Detection Stubs

**Problem:** Six methods declared but returning stubs:
- `parseBlock()` returns nullptr
- `parseInline()` returns nullptr
- `isHeading()`, `isBlockQuote()`, `isCodeBlock()`, `isList()` return false

**Current complexity:** Documents intended design that never materialized. Confuses readers.

**Refactor:**
1. Delete all six stub methods
2. Keep `isListItem(line)` which actually works

**Impact:** Removes ~30 lines of misleading API surface
**Risk:** Low - code is never called
**Workaround deleted:** The fiction of modular parser design

---

### R3. Delete GlyphRun Struct

**Problem:** `GlyphRun` struct declared with `glyphIds`, `positions`, `width`, `height`. TextLayoutObject declares `std::vector<GlyphRun> glyphRuns` but never populates it.

**Current complexity:**
- Dead struct definition
- Dead accessor `getGlyphRuns()` returning empty vector
- Actual hit-testing uses separate `charXOffsets` vector

**Refactor:**
1. Delete `GlyphRun` struct
2. Delete `glyphRuns` member and accessor
3. Document that `charXOffsets` is the hit-test data structure

**Impact:** Removes ~20 lines of dead code
**Risk:** Low - struct is never populated
**Workaround deleted:** The fiction of cached glyph data

---

### R4. Delete Unused Painter Methods

**Problem:** Four methods implemented but never called:
- `paintBorder()` - stub
- `paintLinkUnderline()` - fully implemented, never called
- `isInsideLink()` - fully implemented, never called
- `paintTableCell()` - stub

**Refactor:** Delete all four methods

**Impact:** Removes ~50 lines of dead code
**Risk:** Low - methods are never called
**Workaround deleted:** Abandoned feature experiments

---

### R5. Collapse Rect PaintOp Types

**Problem:** Four structurally-identical operations:
- `DrawRectOp`
- `DrawDebugBorderOp`
- `DrawSelectionRectOp`
- `DrawCaretOp`

All have same fields (rect, color). Differentiated only by type.

**Refactor:**
```cpp
enum class RectRole { Background, DebugBorder, Selection, Caret };
struct DrawRectOp {
    Rect rect;
    Color color;
    RectRole role;  // For potential future role-specific behavior
};
```

**Impact:** Reduces 4 classes to 1, simplifies switch statement
**Risk:** Low - behavior-preserving consolidation
**Workaround deleted:** Type proliferation

---

## P1: High Impact, Medium Risk

### R6. Consolidate Text Authority (Make Mechanism Real)

**Problem:** Three copies of document content with split authority. Every edit requires:
1. `memmove` on inputBuffer
2. `textBuffer->setText(inputBuffer)`
3. `markdownRenderer->setTextBuffer(copy)`

**Refactor option A - Raw buffer wins:**
1. Remove Engine::textBuffer entirely
2. Change MarkdownRenderer to accept `const char*, size_t` instead of TextBuffer
3. Parser works directly on raw buffer

**Refactor option B - TextBuffer wins:**
1. Make TextBuffer authoritative with gap buffer implementation
2. Engine delegates all editing to TextBuffer
3. Remove inputBuffer[10MB]

**Recommendation:** Option A (simpler - acknowledges current reality)

**Impact:** Eliminates dual-write pattern across 18+ sites
**Risk:** Medium - changes core data flow
**Workaround deleted:** 200+ lines of synchronization code

---

### R7. Consolidate Dirty Tracking (Single Writer)

**Problem:** Two parallel dirty-tracking systems:
- Engine: `isDirty()`, `markClean()` (implemented but ignored)
- Shell: `diskContent` string comparison every frame

**Refactor:**
1. Delete Shell's `diskContent` cache
2. Have Shell call `Engine::isDirty()` and `Engine::markClean()`
3. Remove O(n) string comparison from render loop

**Impact:** Removes ~30 lines of parallel state management
**Risk:** Medium - changes lifecycle contract between Shell and Engine
**Workaround deleted:** The fiction that Engine's lifecycle API is unused

---

### R8. Delete InlineLayoutObject and LayoutFlow::Inline (Remove Pretense)

**Problem:**
- `LayoutFlow::Inline` enum value exists
- `InlineLayoutObject` class exists with empty `layout()`
- `LayoutEngine::layoutInlineFlow()` is a stub
- All inline layout happens in TextLayoutObject

**Refactor:**
1. Delete `LayoutFlow::Inline` enum value
2. Delete `InlineLayoutObject` class
3. Delete `layoutInlineFlow()` method
4. Document that TextLayoutObject handles all inline content

**Impact:** Removes ~50 lines of dead infrastructure
**Risk:** Medium - touches type hierarchy
**Workaround deleted:** The fiction of a general inline layout system

---

### R9. Delete Clip PaintOps

**Problem:** `SetClipOp` and `RestoreClipOp` are defined but never emitted by Painter. Rasterizer handles clipping directly.

**Refactor:**
1. Delete `SetClipOp` and `RestoreClipOp` classes
2. Delete `PaintOpType::SetClip` and `PaintOpType::RestoreClip` enum values
3. Remove switch cases in Rasterizer

**Impact:** Removes dead clipping abstraction
**Risk:** Medium - touches paint operation type system
**Workaround deleted:** Unused clipping infrastructure

---

## P2: Medium Impact, Medium Risk

### R10. Extract TextStyle to Shared Header

**Problem:** `TextStyle` enum defined in `markdown_objects.h` (parse layer) but used by:
- paint_operations.h
- glyph_atlas.h
- batch_renderer.h

This creates 3 dependency inversions.

**Refactor:**
1. Create `src/engine/text_style.h` with just the enum
2. Move `TextStyle` definition there
3. Update includes across 6 files

**Impact:** Removes 3 dependency inversions
**Risk:** Medium - widespread include changes
**Workaround deleted:** Parse layer dependency in rendering

---

### R11. Extract CaretState to Shared Header

**Problem:** `CaretState` defined in `markdown_renderer.h` but used by `painter.cpp`. Creates circular awareness.

**Refactor:**
1. Create `src/engine/caret_state.h`
2. Move `CaretState` struct there
3. Update includes

**Impact:** Removes circular dependency
**Risk:** Medium - include changes
**Workaround deleted:** Upward dependency from Painter

---

### R12. Extract Geometry Types to Shared Header

**Problem:** `Point`, `Size`, `Rect` defined in `layout_objects.h` but used throughout.

**Refactor:**
1. Create `src/engine/geometry.h`
2. Move geometric types there
3. Have layout_objects.h include it

**Impact:** Clean type organization
**Risk:** Medium - include changes
**Workaround deleted:** Layout header as geometry source

---

### R13. Pick One Link Representation

**Problem:** Two link models:
- `LinkObject` class (declared, inconsistently used)
- `InlineLinkRange` struct (actual runtime model)

**Refactor:**
1. Delete `LinkObject` class
2. Document that `InlineLinkRange` on parent blocks is the model
3. Update any code that checks for `LinkObject`

**Impact:** Removes dualism
**Risk:** Medium - must verify all link handling
**Workaround deleted:** Parallel link representation

---

### R14. Pick One Inline Formatting Model

**Problem:** Three models:
- Tree nodes (`Bold`, `Italic`, `Underline` in MarkdownObjectType) - never instantiated
- Span annotations (`InlineStyleRange`) - actual runtime
- Bitmask (`TextStyle`) - rendering

**Refactor:**
1. Delete unused `Bold`, `Italic`, `Underline` from MarkdownObjectType enum
2. Document that `InlineStyleRange` is the canonical model
3. `TextStyle` bitmask is rendering-only (derived from spans)

**Impact:** Clarifies single source of truth for formatting
**Risk:** Medium - must verify all formatting paths
**Workaround deleted:** Unused tree node types

---

## P3: Medium Impact, High Risk

### R15. Abstract FreeType Behind Font Interface

**Problem:** `FT_Face` appears in public APIs of:
- LayoutEngine
- TextLayoutObject
- MarkdownRenderer
- Rasterizer
- BatchRenderer
- GlyphAtlas

This leaks platform detail through 5 layers.

**Refactor:**
1. Create `Font` class wrapping `FT_Face`
2. Create `FontProvider` interface for font loading
3. Change all public APIs to use `Font`
4. FreeType details stay internal to Rasterizer/GlyphAtlas

**Impact:** Enables testing without FreeType, future portability
**Risk:** High - large surface area change
**Workaround deleted:** FreeType coupling throughout

---

### R16. Abstract GLFW Input Constants

**Problem:** Engine embeds 260+ lines of GLFW key constants. Cannot be tested without GLFW.

**Refactor:**
1. Create `InputEvent` struct abstracting key/modifier info
2. Shell translates GLFW events to `InputEvent`
3. Engine works only with `InputEvent`

**Impact:** Enables Engine unit testing
**Risk:** High - touches all input handling
**Workaround deleted:** Platform coupling in Engine

---

### R17. Consolidate GL Calls in BatchRenderer

**Problem:** GL calls split across:
- Rasterizer (scissor, texture creation)
- BatchRenderer (blend, programs, VAO)
- GlyphAtlas (unpack alignment, texture upload)

**Refactor:**
1. Add `setScissor()`, `createTexture()` to BatchRenderer
2. Rasterizer delegates all GL to BatchRenderer
3. GlyphAtlas receives texture from BatchRenderer

**Impact:** Single GL authority
**Risk:** High - GL state machine is subtle
**Workaround deleted:** Split GL authority

---

### R18. Extract Image Loading from Layout

**Problem:** `ImageLayoutObject::layout()` performs:
- File system access
- Base64 decoding
- PNG header parsing
- JPEG decompression

I/O hidden in geometry computation.

**Refactor:**
1. Create `ImageLoader` service
2. Load images before layout pass
3. Pass decoded dimensions to ImageLayoutObject

**Impact:** Separates I/O from layout
**Risk:** High - changes image pipeline
**Workaround deleted:** I/O disguised as layout

---

## P4: Low Impact, Low Risk

### R19. Delete File-Based Image Decoder Stubs

**Problem:** `Rasterizer::decodeJpeg(filepath)` and `decodePng(filepath)` are empty TODOs.

**Refactor:** Delete both methods

**Impact:** Removes misleading API
**Risk:** Low - methods do nothing
**Workaround deleted:** Unimplemented file loading

---

### R20. Fix goalColumn Reset Bug

**Problem:** `goalColumn` reset on horizontal movement, defeating its purpose.

**Refactor:** Only update goalColumn on horizontal movement when cursor actually moves horizontally.

**Impact:** Fixes UX bug
**Risk:** Low - localized fix
**Workaround deleted:** N/A (actual bug fix)

---

### R21. Delete Unused image.vert Dependency

**Problem:** `image.frag` has no corresponding vertex shader. Implicitly reuses `text.vert`.

**Refactor:**
1. Either create explicit `image.vert`
2. Or document that `text.vert` is shared

**Impact:** Removes hidden coupling
**Risk:** Low - documentation/clarity
**Workaround deleted:** Implicit shader pairing

---

### R22. Remove Unused Atlas Bind Method

**Problem:** `GlyphAtlas::bind()` is implemented but never called.

**Refactor:** Delete the method

**Impact:** Removes dead code
**Risk:** Low - method unused
**Workaround deleted:** Dead API surface

---

## Execution Order

### Phase 1: Dead Code (P0)
Execute R1-R5 first. These are pure deletions with no behavioral change. Each can be done independently.

**Total removal:** ~200 lines of dead code

### Phase 2: Authority Consolidation (P1)
Execute R6-R9. These fix split authority and remove paper abstractions.

**Prerequisites:** Complete Phase 1 (less code to reason about)

### Phase 3: Type Extraction (P2)
Execute R10-R14. These fix dependency inversions.

**Prerequisites:** Complete Phase 2 (stabilized interfaces)

### Phase 4: Platform Abstraction (P3)
Execute R15-R18. These are high-risk refactors.

**Prerequisites:** Complete Phase 3 (clean type hierarchy)

### Phase 5: Cleanup (P4)
Execute R19-R22. Low-priority polish.

---

## Metrics After Full Execution

| Metric | Before | After |
|--------|--------|-------|
| Paper abstractions | 20 | 0 |
| Dependency inversions | 4 | 0 |
| Workaround code lines | ~950 | ~100 |
| Text copies per edit | 3 | 1 |
| FT_Face in public APIs | 5 layers | 1 layer |
| GLFW in Engine | 260+ lines | 0 |
| Circular dependencies | 4 | 0 |

---

## Risk Mitigation

For each refactor:
1. **Write characterization tests** before changes (capture current behavior)
2. **Refactor in small commits** (easy to bisect)
3. **Run visual regression** after each phase (markdown rendering is visual)
4. **Keep stubs temporarily** with deprecation warnings if uncertain about callers

The safest path: complete all P0 refactors first. They are pure deletions that can only reduce complexity.
