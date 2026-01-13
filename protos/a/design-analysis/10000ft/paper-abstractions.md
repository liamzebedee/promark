# Paper Abstractions Catalog

A comprehensive inventory of declared-but-unrealized abstractions across the codebase. Each entry documents the gap between stated interface and actual behavior, the workaround code that compensates, and the systemic cost.

Ranked by systemic impact: abstractions that force the most workaround code across the widest surface area appear first.

---

## Critical Impact (Forces Pervasive Workarounds)

### 1. TextBuffer Position-Based API

**What was declared:**
```cpp
class TextBuffer {
    void insertText(size_t position, const std::string& text);
    void deleteText(size_t position, size_t length);
    std::string getText() const;
};
```
An editing API that maintains text content with efficient position-based mutations.

**What actually exists:**
The API is implemented but never called. Engine owns a raw `char[10MB]` buffer (`inputBuffer`) and performs all mutations via `memmove`/`memcpy` directly on it.

**Workaround code that compensates:**
Engine uses a dual-write pattern appearing 18+ times:
```cpp
memmove(inputBuffer + pos, inputBuffer + pos + len, ...);  // Direct mutation
textBuffer->setText(std::string(inputBuffer, inputLen));    // Sync via full replacement
markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));  // Forward copy
```
Every editing operation (insert, delete, format, paste, undo) repeats this three-phase synchronization.

**Cost:**
- 18+ synchronization sites in engine.cpp
- Every keystroke triggers O(n) buffer copying (textBuffer->setText copies entire content)
- MarkdownRenderer receives a new TextBuffer instance on every edit via `unique_ptr` transfer ceremony
- Three copies of document state exist simultaneously: `char* inputBuffer`, `Engine::textBuffer`, `MarkdownRenderer::textBuffer`
- Silent divergence is guaranteed if any mutation path forgets the sync pattern
- The `insertText`/`deleteText` API represents wasted design surface that future maintainers must understand is dead

---

### 2. Raw/DOM Position Coordinate System

**What was declared:**
Two coordinate spaces with clean translation:
```cpp
int domToRaw(int domPos) const;
int rawToDOM(int rawPos) const;
```
Engine works in raw positions (byte offsets in markdown source), MarkdownRenderer works in DOM positions (character offsets in rendered content).

**What actually exists:**
Engine performs all cursor manipulation in raw space, but hit-testing returns DOM positions, and selection painting requires DOM positions. Translation happens on every frame and every click:
```cpp
int domCursorPos = markdownRenderer->rawToDOM(cursorPos);  // Every render frame
cursorPos = markdownRenderer->hitTest(x, y);               // Every click - returns raw
```

**Workaround code that compensates:**
MarkdownRenderer contains 350+ lines of tree traversal logic to support coordinate translation:
- `collectTextObjects()` - gathers all text nodes for position mapping
- `collectTextLayoutsWithPos()` - builds DOM-to-layout mapping
- `getTotalDOMLength()` - recomputes what parser already knew
- `domToRaw()`/`rawToDOM()` - per-character offset translation

The renderer duplicates tree traversal that the parser already performed.

**Cost:**
- Every frame performs position translation (O(n) tree traversal)
- Every mouse click performs inverse translation
- Cursor positioning bugs are endemic because two position spaces must stay synchronized
- Layout layer knows about cursor model ("empty lines still occupy 1 DOM position")
- Selection handling requires converting between spaces at paint-time
- Adding new markdown elements requires updating both coordinate translation and layout's DOM-length calculation

---

### 3. LayoutFlow::Inline

**What was declared:**
```cpp
enum class LayoutFlow { Block, Inline };
class InlineLayoutObject : public LayoutObject { ... };
void LayoutEngine::layoutInlineFlow(LayoutObject* container);
```
A complete inline layout system for horizontal text flow with wrapping.

**What actually exists:**
- `layoutInlineFlow()` is a stub containing only `// TODO: implement inline layout`
- `InlineLayoutObject::layout()` is an empty method
- All inline layout happens entirely inside `TextLayoutObject::shapeText()` and `wrapText()`

**Workaround code that compensates:**
TextLayoutObject contains 200+ lines implementing complete text shaping:
- UTF-8 decoding
- Glyph positioning via FreeType
- Line wrapping with word-break detection
- `charXOffsets` vector for hit-testing
- Style range handling for bold/italic/code spans

The engine type-switches to decide whether to delegate or handle directly:
```cpp
if (object->isAtomic()) -> call layout()
if (object->getFlow() == Block) -> layoutBlockFlow() recursion
if (object->getFlow() == Inline) -> call layout()  // which does nothing
```

**Cost:**
- TextLayoutObject is a god class (text shaping belongs in layout engine or dedicated shaper)
- No reusable inline layout for non-text inline elements
- Images cannot flow inline with text despite `ImageLayoutObject` existing
- Adding inline elements (e.g., inline code blocks, emoji) requires duplicating shaping logic
- The `InlineLayoutObject` class and `layoutInlineFlow()` method are pure architectural weight

---

### 4. Three Inline Formatting Models

**What was declared:**
Three representations of bold/italic/underline exist:
1. Tree nodes: `MarkdownObjectType::Bold`, `Italic`, `Underline` enum values
2. Span annotations: `InlineStyleRange` struct stored on parent block nodes
3. Bitmask flags: `TextStyle` enum (Bold=1, Italic=2, Code=4) for rendering

**What actually exists:**
- Tree node types for formatting are defined but never instantiated
- Parser populates `InlineStyleRange` spans on paragraph/heading parents
- Text nodes have no knowledge of their own formatting
- Layout retrieves ranges via `getStyleRanges()` from source `MarkdownObject`
- Rendering layer uses `TextStyle` bitmask for font selection

**Workaround code that compensates:**
Every layer must understand the "real" formatting model (span annotations):
```cpp
// Parser builds spans on parent
paragraph->addStyleRange(InlineStyleRange{start, length, StyleType::Bold});

// Layout retrieves spans, ignoring tree structure
auto ranges = sourceObject->getStyleRanges();

// Painter converts spans to TextStyle bitmask
TextStyle style = TextStyle::Normal;
if (range.style == StyleType::Bold) style |= TextStyle::Bold;
```

**Cost:**
- Tree node types for formatting are dead code that documents a design never implemented
- `markdown_objects.h` exports types never used, confusing readers
- Parser-layout coupling: layout must understand parser's internal span format
- Rendering imports `markdown_objects.h` solely for `TextStyle` enum
- Adding new formatting (strikethrough, highlight) requires changes in all three representations
- No way to traverse formatted regions as tree - must iterate flat span list

---

### 5. Engine Dirty/Close API

**What was declared:**
```cpp
bool Engine::isDirty() const;
void Engine::markClean();
bool Engine::shouldClose() const;
```
Clean lifecycle management where Engine tracks unsaved state.

**What actually exists:**
These methods are implemented but completely bypassed by `edit.cpp`:
```cpp
// edit.cpp implements parallel dirty tracking:
std::string diskContent;  // Shadow of last-saved content
bool dirty = (engine->getContent() != diskContent);
```
Every frame, the shell calls `getContent()` which copies the entire buffer to a string, then compares against its own cached version.

**Workaround code that compensates:**
- Shell maintains `diskContent` string parallel to Engine state
- Every frame performs O(n) string comparison
- Window title updates based on shell's dirty flag, not Engine's
- `shouldClose()` is never called; shell manages close confirmation via its own dirty check
- `markClean()` is never called; shell updates `diskContent` after save

**Cost:**
- Two dirty-tracking systems that can diverge
- Every frame copies entire document to string for comparison
- Engine's lifecycle API is architectural decoration
- Future shells must rediscover that Engine's API doesn't work
- Close confirmation dialog logic duplicated in shell, not reusable

---

## High Impact (Forces Multi-Site Workarounds)

### 6. PaintOp Display List

**What was declared:**
`paint_operations.h` defines a comprehensive command protocol:
- `DrawRectOp`, `DrawTextOp`, `DrawImageOp`, `DrawLineOp`
- `SetClipOp`, `RestoreClipOp`
- `PaintOpType` enum for dispatch

**What actually exists:**
BatchRenderer ignores the display list entirely. It provides raw primitives:
- `drawQuad()`, `drawRect()`, `drawImage()`, `drawText()`

Rasterizer manually type-switches every PaintOp:
```cpp
for (const auto& op : displayList) {
    switch (op->getType()) {
        case PaintOpType::DrawRect:
            batchRenderer->drawRect(static_cast<DrawRectOp*>(op)->rect, ...);
            break;
        case PaintOpType::DrawText:
            // Extract all fields, call batchRenderer->drawText()
            break;
        // ... 10 more cases
    }
}
```

**Workaround code that compensates:**
- 50+ line switch statement in Rasterizer::rasterize()
- Each new PaintOp type requires: struct definition, enum value, switch case, BatchRenderer method
- `SetClipOp`/`RestoreClipOp` are defined but never emitted
- Four structurally-identical rect operations (DrawRect, DrawDebugBorder, DrawSelection, DrawCaret) exist when one with role enum would suffice

**Cost:**
- Display list is pure overhead (constructed then manually disassembled)
- No batching optimization possible (could sort by type, defer state changes)
- Inheritance exists for type marking only; `std::variant` would be more honest
- Clipping infrastructure is dead weight
- Each new visual element requires 4-5 code locations to update

---

### 7. GlyphRun Struct

**What was declared:**
```cpp
struct GlyphRun {
    std::vector<uint32_t> glyphIds;
    std::vector<Point> positions;
    float width;
    float height;
};
```
With `TextLayoutObject` storing `std::vector<GlyphRun> glyphRuns` and exposing `getGlyphRuns()`.

**What actually exists:**
`shapeText()` clears the vector but never populates it. Actual glyph positioning data goes to a completely different structure:
```cpp
std::vector<float> charXOffsets;  // Actually used for hit-testing
```

**Workaround code that compensates:**
- Hit-testing uses `charXOffsets` vector
- Rendering re-shapes text at rasterization time (BatchRenderer::drawText does UTF-8 decode and glyph lookup again)
- No shared glyph data between layout and render passes

**Cost:**
- Text is shaped twice: once in layout (partial, for metrics), once in render (full, for display)
- `GlyphRun` struct is pure dead weight
- `getGlyphRuns()` accessor exists, returning always-empty vector
- Future optimization (cache shaped glyphs) would require understanding this isn't actually used

---

### 8. Link Representation Dualism

**What was declared:**
Two link representations exist:
```cpp
class LinkObject : public MarkdownObject {
    std::string url;
};
struct InlineLinkRange {
    int start;
    int length;
    std::string url;
};
```

**What actually exists:**
- `LinkObject` class exists but isn't the runtime model
- `InlineLinkRange` spans are stored on parent paragraph/heading nodes
- Link detection uses span ranges, not tree traversal

**Workaround code that compensates:**
```cpp
// Hit-testing iterates link ranges, not child nodes:
for (const auto& linkRange : paragraph->getLinkRanges()) {
    if (offset >= linkRange.start && offset < linkRange.start + linkRange.length) {
        return linkRange.url;
    }
}
```

**Cost:**
- Two mental models for links
- Tree structure doesn't represent link boundaries
- Cannot style link children independently (links contain raw text, not styled children)
- `LinkObject` class maintains illusion of unused capability

---

### 9. Clipboard Platform Abstraction

**What was declared:**
```cpp
class Clipboard {
    static void setHandlers(GetTextFunc getFunc, SetTextFunc setFunc);
    static bool hasCustomHandlers();
    // Platform-agnostic clipboard access
};
```

**What actually exists:**
`clipboard.cpp` unconditionally includes GLFW:
```cpp
#include <GLFW/glfw3.h>
```
The runtime callback mechanism exists but compile-time dependency remains. "Platform-agnostic" means "GLFW is always required."

**Workaround code that compensates:**
- Web builds must stub GLFW headers
- `hasCustomHandlers()` exists solely because abstraction leaks - proper abstraction wouldn't require querying handler state
- Static callback pointers (`s_getText`, `s_setText`) with no synchronization
- Handler mutation during operation is undefined behavior

**Cost:**
- Clipboard cannot be used without GLFW linked
- Two code paths (custom handlers, GLFW direct) must be maintained
- Thread safety absent for static state
- `hasCustomHandlers()` is a design smell indicating leaky abstraction

---

## Moderate Impact (Localized Workarounds)

### 10. Base LayoutObject Virtual Methods

**What was declared:**
```cpp
class LayoutObject {
    virtual void layout();
    virtual Size computeIntrinsicSize();
};
```
Polymorphic layout dispatch.

**What actually exists:**
- `layout()` is empty stub, every subclass overrides
- `computeIntrinsicSize()` returns zero; only Text and Image override
- Engine type-switches to decide whether to call `layout()`:
```cpp
if (object->isAtomic()) -> call layout()
if (table/list) -> call layout()
if (block) -> layoutBlockFlow()  // recurses, sometimes calls layout()
```

**Workaround code that compensates:**
Virtual dispatch exists but engine explicitly checks types to decide dispatch strategy. Base class exists for tree structure, not behavioral polymorphism.

**Cost:**
- Inheritance hierarchy without meaningful base behavior
- New layout types require understanding dispatch rules encoded in engine
- Virtual method overhead without polymorphism benefit

---

### 11. Dirty Flag Optimization (Incomplete)

**What was declared:**
```cpp
bool needsReparse;
bool needsRelayout;
bool needsRepaint;
```
Three-phase dirty tracking for incremental updates.

**What actually exists:**
- `rasterize()` runs unconditionally every frame - no `needsRasterize` flag
- Height changes trigger relayout despite markdown layout depending only on width
- Partial relayout isn't possible - flags control entire-pipeline re-execution

**Workaround code that compensates:**
Every frame calls `rasterize()` regardless of dirty state. GPU work cannot be skipped.

**Cost:**
- GPU commands submitted every frame even when display hasn't changed
- Scroll-only updates re-rasterize entire document
- Memory bandwidth wasted re-uploading unchanged glyphs

---

### 12. Parser Detection Predicates

**What was declared:**
```cpp
bool parseBlock(const std::string& text, size_t position);  // Returns nullptr
bool parseInline(const std::string& text, size_t position); // Returns nullptr
bool isHeading(const std::string& text, size_t position);   // Returns false
bool isBlockQuote(const std::string& text, size_t position); // Returns false
bool isCodeBlock(const std::string& text, size_t position);  // Returns false
bool isList(const std::string& text, size_t position);       // Returns false
```

**What actually exists:**
All return stub values. Actual implementation is a 516-line monolithic `parseDocument()` with inline detection.

**Workaround code that compensates:**
```cpp
// Instead of: if (isHeading(text, pos)) parseHeading(...)
// Actual:
if (line.length() > 0 && line[0] == '#') {
    // 50 lines of heading parsing inline
}
```
Every detection is inlined into the main parse loop.

**Cost:**
- 516-line method impossible to test incrementally
- Detection logic duplicated (once in stubbed predicate, once inline)
- `isListItem(line)` exists with different signature than `isList(text, pos)` - design abandoned mid-refactor
- API documents intended design that never materialized

---

### 13. Atlas Style/Mono Fields

**What was declared:**
```cpp
struct AtlasKey {
    uint32_t codepoint;
    int size;
    TextStyle style;  // Bold, Italic, etc.
    bool mono;
};
```
Atlas handles font variants via key discrimination.

**What actually exists:**
Atlas never uses `style` or `mono` for rasterization. They serve only as cache keys. Font face selection happens in Rasterizer, which passes the appropriate `FT_Face`. The atlas just renders whatever face it receives.

**Workaround code that compensates:**
Rasterizer must select correct font face before calling atlas:
```cpp
FT_Face face = isMono ? monoFace : (isBold ? boldFace : regularFace);
glyph = atlas->get(face, codepoint, size, style, mono);  // style/mono are key-only
```

**Cost:**
- `AtlasKey` fields imply capability that doesn't exist
- Font selection logic duplicated in every atlas caller
- Changing font selection rules requires understanding atlas doesn't participate

---

### 14. Image Tinting (image.frag)

**What was declared:**
```glsl
gl_FragColor = texColor * v_color;  // Color tinting support
```

**What actually exists:**
BatchRenderer always passes white `(1,1,1,1)`. The multiplication is a no-op.

**Workaround code that compensates:**
No workaround - capability is simply unused. Image dimming, overlays, and fades would require:
- BatchRenderer API changes
- Painter changes to emit color
- New PaintOp field

**Cost:**
- Shader capability documents unrealized feature
- Future implementer must discover API doesn't expose shader's capability

---

### 15. Batching (drawImage bypass)

**What was declared:**
`begin()`/`flush()` frame model for batched GL rendering.

**What actually exists:**
`drawImage()` calls `flush()` immediately, then performs immediate-mode rendering. Images cannot batch because they use external textures.

Additionally, `textured` boolean causes thrashing:
```cpp
if (!textured && !vertices.empty()) { flush(); }
textured = true;
// Later:
if (textured && !vertices.empty()) { flush(); }
textured = false;
```

**Workaround code that compensates:**
None - documents with alternating text and images flush on every transition.

**Cost:**
- Image-heavy documents lose all batching benefit
- Mixed content (text, rect, text, image, text) worst case
- Batching architecture exists but doesn't deliver claimed benefit

---

## Lower Impact (Isolated Dead Code)

### 16. Painter Stub Methods

| Method | Status |
|--------|--------|
| `paintBorder()` | Stub with "not yet implemented" comment |
| `paintLinkUnderline()` | Never called; underlines painted inline |
| `isInsideLink()` | Never called; range-based detection used |
| `paintTableCell()` | Stub with "could be used if needed" comment |

**Cost:** Dead weight in API surface. Each represents abandoned design direction.

---

### 17. findClosingDelimiter Dead Code

```cpp
while (pos < line.length()) {
    size_t found = line.find(delim, pos);
    return found;  // Returns immediately, loop never iterates
}
```
Comment acknowledges escape handling should exist; implementation returns on first match.

**Cost:** Bug for escaped delimiters. Technical debt documented but not addressed.

---

### 18. File-Based Image Decoders

```cpp
std::vector<uint8_t> Rasterizer::decodeJpeg(const std::string& filepath); // Empty TODO
std::vector<uint8_t> Rasterizer::decodePng(const std::string& filepath);  // Empty TODO
```
Memory-based decoders work; file-based declared but empty.

**Cost:** File-path images silently fail. Workaround is always embedding base64.

---

### 19. goalColumn for Vertical Navigation

**What was declared:**
```cpp
int goalColumn;  // "remembered for vertical nav"
```

**What actually exists:**
Reset by horizontal movement, defeating purpose. Arrow-up/down after arrow-right loses column memory.

**Cost:** UX bug. Vertical cursor navigation doesn't preserve horizontal position as expected.

---

### 20. UndoState Naming

**What was declared:**
`UndoState` struct name implies undo/redo system.

**What actually exists:**
Only undo exists. Redo is not implemented.

**Cost:** Misleading name. Future implementer expects redo, finds none.

---

## Summary: Systemic Debt Distribution

| Severity | Count | Primary Impact |
|----------|-------|----------------|
| Critical | 5 | Force workaround patterns at 10+ call sites each |
| High | 4 | Force workarounds at 3-10 call sites |
| Moderate | 6 | Force localized workarounds or waste |
| Lower | 5 | Dead code / misleading naming |

**Most expensive abstractions by workaround LOC:**
1. TextBuffer API bypass: ~200 lines of sync code
2. Raw/DOM translation: ~350 lines in MarkdownRenderer
3. Inline layout stub: ~200 lines in TextLayoutObject
4. Three formatting models: ~100 lines of conversion scattered
5. PaintOp display list: ~100 lines of manual dispatch

Total: ~950 lines of workaround code compensating for paper abstractions that could be eliminated by either implementing the declared design or removing the false interface.
