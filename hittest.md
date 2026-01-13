# Hit Testing Architecture Redesign

## Current Architecture Analysis

### Current State Flow (Problems Identified)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        CURRENT ARCHITECTURE                             │
│                     (Bidirectional Dependencies)                        │
└─────────────────────────────────────────────────────────────────────────┘

     ┌──────────┐         ┌──────────────────┐         ┌─────────────┐
     │  Engine  │◄───────►│ MarkdownRenderer │◄───────►│LayoutEngine│
     │          │         │                  │         │             │
     │ cursorPos│         │ rawToDOM()       │         │ layoutTree  │
     │ scrollOff│         │ domToRaw()       │         │             │
     │ selection│         │ hitTest()        │         │             │
     └────┬─────┘         └────────┬─────────┘         └─────────────┘
          │                        │
          │ queries positions      │ queries for
          │ EVERY FRAME           │ caret animation
          ▼                        ▼
    Triple rawToDOM()         getCursorXY()
    conversions per frame     getCursorY()
```

**Key Problems:**

1. **Bidirectional coupling**: Engine queries Renderer for positions, Renderer holds state
2. **Redundant conversions**: 3+ `rawToDOM()` calls per frame (lines 156-159 in engine.cpp)
3. **Hit testing only handles text**: Images, blockquotes miss direct hit detection
4. **No unified hit result**: Different code paths for text vs links vs images
5. **Scroll validation uses stale contentHeight**: One-frame latency on content changes
6. **TextBuffer copied on every keystroke**: O(n) allocation per character

### Current Hit Test Coverage

| Content Type | Hit Test Support | File:Line |
|--------------|------------------|-----------|
| **Text** | Full (char-level) | markdown_renderer.cpp:201-362 |
| **Links** | Separate function | markdown_renderer.cpp:458-505 |
| **Images** | Falls through to text | — |
| **Blockquotes** | Transparent (hits text inside) | — |
| **Lists** | Transparent (hits text inside) | — |
| **Tables** | Transparent (hits text inside) | — |
| **Code blocks** | Transparent (hits text inside) | — |

---

## Proposed Architecture: One-Way State Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      PROPOSED ARCHITECTURE                              │
│                    (Unidirectional Data Flow)                           │
└─────────────────────────────────────────────────────────────────────────┘

                              INPUT EVENTS
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                            ENGINE (Controller)                           │
│  • Receives raw input (mouse, keyboard)                                  │
│  • Owns: inputBuffer, cursorPos, selection, scrollOffset                 │
│  • Dispatches actions, does NOT query renderer for positions             │
└──────────────────────────────────────────────────────────────────────────┘
                                   │
                                   │ setText(), setViewport()
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         DOCUMENT MODEL (Source of Truth)                 │
│  • Owns: raw text, parsed DOM, position mappings                         │
│  • Provides: rawToDOM(), domToRaw() (cached/incremental)                 │
│  • Invalidation flags cascade: parse → layout → paint                    │
└──────────────────────────────────────────────────────────────────────────┘
                                   │
                                   │ DOM tree
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         LAYOUT ENGINE (Pure Function)                    │
│  • Input: DOM tree, viewport size                                        │
│  • Output: Layout tree with positions, HitTestIndex                      │
│  • Builds spatial index during layout (not on demand)                    │
└──────────────────────────────────────────────────────────────────────────┘
                                   │
                                   │ Layout tree + HitTestIndex
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         HIT TEST SERVICE (Stateless)                     │
│  • Input: (x, y) in content space, HitTestIndex                          │
│  • Output: HitTestResult with rich type info                             │
│  • Uniform handling for ALL content types                                │
└──────────────────────────────────────────────────────────────────────────┘
                                   │
                                   │ HitTestResult
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         PAINTER (Pure Function)                          │
│  • Input: Layout tree, CaretState, Selection                             │
│  • Output: DisplayList (paint operations)                                │
└──────────────────────────────────────────────────────────────────────────┘
                                   │
                                   │ DisplayList
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         RASTERIZER (GPU Interface)                       │
│  • Input: DisplayList, scrollOffset                                      │
│  • Output: Rendered frame                                                │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Unified Hit Test Design

### HitTestResult Structure

```cpp
// hit_test.h (NEW FILE)

enum class HitTargetType {
    None,           // Click in empty space
    Text,           // Text content (paragraph, heading, etc.)
    Image,          // Image element
    Link,           // Clickable link
    ListMarker,     // List bullet/number
    BlockquoteBar,  // Blockquote decoration
    CodeBlock,      // Code block content
    TableCell,      // Table cell
    Scrollbar,      // UI scrollbar
    Toolbar         // Toolbar area
};

struct HitTestResult {
    HitTargetType type = HitTargetType::None;

    // Position information
    int rawPosition = -1;           // Position in raw text buffer
    int domPosition = -1;           // Position in DOM

    // Containing element info
    const LayoutObject* layout = nullptr;
    const MarkdownObject* domNode = nullptr;

    // Type-specific data
    std::string linkUrl;            // For Link type
    int listIndex = -1;             // For ListMarker type
    int tableRow = -1;              // For TableCell type
    int tableCol = -1;

    // Geometry
    Rect elementBounds;             // Bounds of hit element
    Point clickPoint;               // Original click point

    // Flags
    bool isAtomicElement = false;   // Image, horizontal rule, etc.
    bool isEditable = true;         // Can cursor be placed here?
    bool isInteractive = false;     // Link, checkbox, etc.
};
```

### HitTestIndex (Spatial Index Built During Layout)

```cpp
// hit_test_index.h (NEW FILE)

struct HitRegion {
    Rect bounds;
    HitTargetType type;
    const LayoutObject* layout;
    int domStart;
    int domEnd;

    // For text regions: character-level data
    std::vector<float> charXOffsets;  // Shared from TextLayoutObject
    std::vector<LineInfo> lines;       // Shared from TextLayoutObject
};

class HitTestIndex {
public:
    // Build index from layout tree (called once after layout)
    void build(const LayoutObject* root);

    // Query methods
    HitTestResult hitTest(float x, float y) const;
    std::vector<HitTestResult> hitTestAll(float x, float y) const;  // All overlapping
    HitTestResult hitTestNearest(float x, float y) const;           // Fallback

private:
    // Regions sorted by Y then X for efficient lookup
    std::vector<HitRegion> regions;

    // Interval tree or R-tree for O(log n) lookup (optional optimization)
    // For now: linear scan with early exit is sufficient
};
```

### Unified Hit Test Algorithm

```cpp
// hit_test_service.cpp (NEW FILE)

HitTestResult HitTestService::hitTest(float x, float y, const HitTestIndex& index) const {
    HitTestResult result;
    result.clickPoint = {x, y};

    // 1. Check all regions for direct hit
    for (const auto& region : index.regions) {
        if (!region.bounds.contains(x, y)) continue;

        switch (region.type) {
            case HitTargetType::Image:
                return makeImageResult(region, x, y);

            case HitTargetType::Link:
                return makeLinkResult(region, x, y);

            case HitTargetType::Text:
                return makeTextResult(region, x, y);  // Character-level

            case HitTargetType::ListMarker:
                return makeListMarkerResult(region, x, y);

            case HitTargetType::BlockquoteBar:
                return makeBlockquoteResult(region, x, y);

            case HitTargetType::CodeBlock:
                return makeCodeBlockResult(region, x, y);

            case HitTargetType::TableCell:
                return makeTableCellResult(region, x, y);
        }
    }

    // 2. Fallback: find nearest text region by Y, then X
    return findNearestTextPosition(x, y, index);
}

HitTestResult HitTestService::makeTextResult(const HitRegion& region, float x, float y) const {
    HitTestResult result;
    result.type = HitTargetType::Text;
    result.layout = region.layout;
    result.elementBounds = region.bounds;

    // Find line by Y
    int lineIdx = findLineAtY(region, y);
    const auto& line = region.lines[lineIdx];

    // Find character by X within line
    float relX = x - region.bounds.position.x;
    int charIdx = findCharAtX(region, lineIdx, relX);

    result.domPosition = region.domStart + charIdx;
    result.rawPosition = domToRaw(result.domPosition);  // Cached lookup
    result.isEditable = true;

    return result;
}

HitTestResult HitTestService::makeImageResult(const HitRegion& region, float x, float y) const {
    HitTestResult result;
    result.type = HitTargetType::Image;
    result.layout = region.layout;
    result.elementBounds = region.bounds;
    result.isAtomicElement = true;

    // Cursor goes before or after image based on X position
    float midX = region.bounds.position.x + region.bounds.size.width / 2;
    if (x < midX) {
        result.domPosition = region.domStart;      // Before image
    } else {
        result.domPosition = region.domStart + 1;  // After image
    }
    result.rawPosition = domToRaw(result.domPosition);
    result.isEditable = true;

    return result;
}
```

---

## Engine Changes for One-Way Flow

```cpp
// engine.cpp - MODIFIED handleMouse()

void Engine::handleMouse(int button, int action, int mods, double x, double y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        // 1. Transform coordinates once
        float contentX = static_cast<float>(x);
        float contentY = static_cast<float>(y - TOOLBAR_HEIGHT) + scrollOffset;

        // 2. Single unified hit test (replaces multiple queries)
        HitTestResult hit = hitTestService->hitTest(contentX, contentY, hitTestIndex);

        // 3. Handle result based on type (no renderer queries needed)
        switch (hit.type) {
            case HitTargetType::Link:
                if (hit.isInteractive) {
                    openUrl(hit.linkUrl);
                    return;
                }
                // Fall through to position cursor
                [[fallthrough]];

            case HitTargetType::Text:
            case HitTargetType::CodeBlock:
            case HitTargetType::TableCell:
                positionCursor(hit.rawPosition, hit.domPosition);
                break;

            case HitTargetType::Image:
                positionCursor(hit.rawPosition, hit.domPosition);
                // Could also: select image, show resize handles, etc.
                break;

            case HitTargetType::ListMarker:
                // Could: toggle checkbox, select list item
                positionCursor(hit.rawPosition, hit.domPosition);
                break;

            case HitTargetType::BlockquoteBar:
                // Select entire blockquote
                selectBlockquote(hit.domNode);
                break;

            case HitTargetType::None:
                // Click in margin - find nearest text
                hit = hitTestService->hitTestNearest(contentX, contentY, hitTestIndex);
                positionCursor(hit.rawPosition, hit.domPosition);
                break;
        }

        // 4. Caret position is PUSHED, not queried
        caretTargetX = hit.elementBounds.position.x;  // From hit result
        caretTargetY = hit.elementBounds.position.y;
        // Or compute from layout directly without querying renderer
    }
}

void Engine::positionCursor(int rawPos, int domPos) {
    cursorPos = rawPos;
    cachedDomPos = domPos;  // Cache to avoid re-conversion
    selectionStart = rawPos;
    selectionEnd = rawPos;
    hasSelection = false;

    // Caret animation snaps on click
    // Position computed from hit result, not queried
}
```

---

## Index Building During Layout

```cpp
// layout_engine.cpp - MODIFIED

std::pair<LayoutObject*, HitTestIndex> LayoutEngine::createLayoutTree(
    MarkdownObject* domRoot,
    const Size& viewport
) {
    auto layoutRoot = createLayoutObject(domRoot);
    performLayout(layoutRoot.get(), {viewport.width, viewport.height});

    // Build spatial index as part of layout phase
    HitTestIndex index;
    buildHitTestIndex(layoutRoot.get(), index, 0);  // domOffset starts at 0

    return {layoutRoot.release(), std::move(index)};
}

void LayoutEngine::buildHitTestIndex(
    const LayoutObject* layout,
    HitTestIndex& index,
    int& domOffset
) {
    const Rect& rect = layout->getRect();

    // Add region based on type
    if (auto* text = dynamic_cast<const TextLayoutObject*>(layout)) {
        HitRegion region;
        region.bounds = rect;
        region.type = HitTargetType::Text;
        region.layout = layout;
        region.domStart = domOffset;
        region.domEnd = domOffset + text->getDOMLength();
        region.charXOffsets = text->getCharXOffsetsRef();  // Shared, not copied
        region.lines = text->getLinesRef();

        // Check for links within this text
        for (const auto& link : text->getLinkRanges()) {
            HitRegion linkRegion;
            linkRegion.bounds = computeLinkBounds(text, link);
            linkRegion.type = HitTargetType::Link;
            linkRegion.layout = layout;
            linkRegion.domStart = domOffset + link.startChar;
            linkRegion.domEnd = domOffset + link.endChar;
            linkRegion.linkUrl = link.url;
            index.addRegion(std::move(linkRegion));
        }

        index.addRegion(std::move(region));
        domOffset += text->getDOMLength();

    } else if (auto* img = dynamic_cast<const ImageLayoutObject*>(layout)) {
        HitRegion region;
        region.bounds = rect;
        region.type = HitTargetType::Image;
        region.layout = layout;
        region.domStart = domOffset;
        region.domEnd = domOffset + 1;
        index.addRegion(std::move(region));
        domOffset += 1;

    } else if (isBlockquote(layout)) {
        // Add bar region for blockquote decoration
        HitRegion barRegion;
        barRegion.bounds = {rect.position.x - 15, rect.position.y, 3, rect.size.height};
        barRegion.type = HitTargetType::BlockquoteBar;
        barRegion.layout = layout;
        index.addRegion(std::move(barRegion));
    }

    // Recurse to children
    for (const auto& child : layout->getChildren()) {
        buildHitTestIndex(child.get(), index, domOffset);
    }
}
```

---

## Coordinate Systems

### Current Coordinate Spaces

| Space | Origin | Used By |
|-------|--------|---------|
| **Screen** | Top-left of window | GLFW input, main.cpp |
| **Content** | Top-left of document | Layout, painting, hit testing |
| **DOM** | Character index in rendered text | Position mapping |
| **Raw** | Byte index in markdown buffer | Editing operations |

### Coordinate Transformations

```
Screen → Content:
    contentX = screenX
    contentY = screenY - TOOLBAR_HEIGHT + scrollOffset

Content → Screen:
    screenX = contentX
    screenY = contentY + TOOLBAR_HEIGHT - scrollOffset

DOM → Raw:
    rawPos = domToRaw(domPos)  // Accounts for markdown syntax

Raw → DOM:
    domPos = rawToDOM(rawPos)  // Inverse mapping
```

---

## Summary of Changes

| Component | Current | Proposed |
|-----------|---------|----------|
| **Hit test location** | MarkdownRenderer | Separate HitTestService |
| **Hit test result** | int (raw position) | Rich HitTestResult struct |
| **Content types** | Text-only + separate link check | All types unified |
| **Spatial index** | None (linear scan) | HitTestIndex built at layout |
| **Position queries** | Engine queries Renderer | Renderer pushes to Engine |
| **DOM-Raw mapping** | Per-frame conversion | Cached in hit result |
| **Caret position** | Queried via getCursorXY() | Computed from hit result |
| **State flow** | Bidirectional | Unidirectional |

---

## Implementation Plan

### Phase 1: Core Infrastructure
1. Create `hit_test.h` with `HitTargetType` and `HitTestResult`
2. Create `hit_test_index.h` with `HitRegion` and `HitTestIndex`
3. Create `hit_test_service.h/cpp` with query methods

### Phase 2: Index Building
1. Modify `LayoutEngine` to build `HitTestIndex` during layout
2. Add region creation for each layout object type
3. Compute link bounds from text layout character offsets

### Phase 3: Engine Integration
1. Replace `markdownRenderer->hitTest()` calls with `HitTestService`
2. Remove redundant `rawToDOM()` calls
3. Cache DOM position in hit result

### Phase 4: Remove Legacy
1. Deprecate `MarkdownRenderer::hitTest()`
2. Deprecate `MarkdownRenderer::getLinkAtPosition()`
3. Remove per-frame position queries

---

## File Locations Reference

### Current Implementation
| File | Purpose | Key Lines |
|------|---------|-----------|
| `engine.cpp` | Mouse handling | 498-588 |
| `markdown_renderer.cpp` | Hit testing | 201-362 |
| `markdown_renderer.cpp` | Link detection | 458-505 |
| `layout_objects.h` | Geometry types | 8-23 |
| `layout_engine.cpp` | Layout tree | 51-230 |

### New Files
| File | Purpose |
|------|---------|
| `hit_test.h` | HitTargetType, HitTestResult |
| `hit_test_index.h` | HitRegion, HitTestIndex |
| `hit_test_service.h` | HitTestService class |
| `hit_test_service.cpp` | Hit test implementation |
