# Claude's Architecture Analysis - Markdown Editor v1

## Executive Summary

The editor works but has accumulated complexity that violates the "make it simple" principle. The core issues are:

1. **Engine is a God Class** - 1800+ lines handling 15+ responsibilities
2. **Dual coordinate systems** - Raw positions vs DOM positions require constant conversion
3. **State duplication** - Cursor/selection state exists in both Engine and MarkdownRenderer
4. **Dual rendering pipelines** - Raw mode and Markdown mode are separate systems
5. **O(n) operations per frame** - Tree traversals for cursor/selection on every keystroke

---

## Current Pipeline (What Works)

```
TextBuffer (raw markdown string)
    ↓ parse()
MarkdownObjects (DOM tree with rawStart/rawEnd tracking)
    ↓ createLayoutTree() + performLayout()
LayoutObjects (positioned tree with glyph metrics)
    ↓ paint()
DisplayList (DrawTextOp, DrawRectOp, etc.)
    ↓ rasterize()
OpenGL → Screen
```

**What's good:**
- Clean separation: Parse → Layout → Paint → Rasterize
- Dirty flag caching: needsReparse, needsRelayout, needsRepaint
- Immutable parse trees reduce bugs
- Command buffer pattern (DisplayList) decouples painting from GPU

---

## Critical Architectural Flaws

### 1. Engine God Class (1800 lines, 15+ responsibilities)

**Current responsibilities:**
- Text buffer management (10MB char array)
- Keyboard input dispatch (250+ lines of if-chains)
- Mouse/scroll handling
- Text navigation (word boundaries, line start/end)
- Selection management (scattered across 10+ methods)
- Undo system (naive full-text copies)
- Clipboard operations
- Caret animation state
- **Toolbar UI rendering** (doesn't belong here)
- **Raw text rendering** (parallel pipeline!)
- **Font loading** (should be centralized)
- **FreeType initialization**
- Link URL opening
- Scroll management
- Dirty state tracking

**Violation:** Single Responsibility Principle completely ignored.

### 2. Dual Coordinate Systems

The system maintains two parallel position systems:

**Raw positions** (Engine):
```cpp
int cursorPos;        // byte index in inputBuffer
int selectionStart;   // byte index
int selectionEnd;     // byte index
```

**DOM positions** (MarkdownRenderer):
```cpp
struct CaretState {
    int cursorPosition;   // character index in flattened DOM
    int selectionStart;
    int selectionEnd;
};
```

**Every frame**, Engine converts:
```cpp
int domCursorPos = markdownRenderer->rawToDOM(cursorPos);
int domSelStart = markdownRenderer->rawToDOM(selectionStart);
int domSelEnd = markdownRenderer->rawToDOM(selectionEnd);
```

**Problems:**
- `rawToDOM()` is O(n) tree traversal
- Gap handling between objects is ambiguous
- Off-by-one errors waiting to happen
- No compile-time safety for position types

### 3. Dual Rendering Pipelines

**Pipeline A - Markdown mode:**
```
Engine.inputBuffer → MarkdownRenderer → DisplayList → GPU
```

**Pipeline B - Raw mode (in Engine!):**
```
Engine.inputBuffer → Engine.renderRawText() → BatchRenderer → GPU
```

Raw mode has its own:
- `renderRawText()` - 300+ lines duplicating layout logic
- `hitTestRaw()` - duplicate hit testing
- `getCursorYRaw()` - duplicate cursor positioning
- Syntax highlighting colors hardcoded

**Problem:** Two systems that can diverge. Maintenance burden doubled.

### 4. State Duplication

Cursor state exists in **two places**:

**Engine:**
```cpp
int cursorPos;
int selectionStart, selectionEnd;
bool hasSelection;
float caretAnimX, caretAnimY;
float caretTargetX, caretTargetY;
double lastBlinkTime;
bool caretVisible;
int goalColumn;  // for vertical navigation
```

**MarkdownRenderer:**
```cpp
CaretState caretState;  // duplicates cursor/selection!
```

**Synchronization:** Manual. Engine must call `setCaretState()` every frame. If forgotten, visual state diverges.

### 5. O(n) Operations Per Frame

Every keystroke triggers:

1. `rawToDOM()` - O(n) tree traversal
2. `findLayoutForPosition()` - O(n) layout traversal
3. `collectContentLayouts()` - O(n) for selection painting
4. Style/link lookups were O(n*m) until recently optimized

For a 10,000 line document, that's 30,000+ pointer dereferences per keystroke.

---

## Data Flow Issues

### Text Stored in 5 Places

1. **TextBuffer** - raw markdown string
2. **MarkdownObjects** - display text (without syntax)
3. **TextLayoutObject** - glyph runs and line info
4. **Painter** - re-extracts text during painting
5. **DrawTextOp** - stores text snippets

Changes must propagate through all layers correctly.

### Position Mapping is Convoluted

To paint caret at cursor position:
```
Engine.cursorPos (raw)
  → rawToDOM() [O(n)]
MarkdownRenderer.cursorPosition (DOM)
  → findLayoutForPosition() [O(n)]
Painter finds TextLayoutObject
  → getLineForChar()
  → getCharXOffsetInLine()
  → generates DrawCaretOp
  → rasterize()
```

Each hop is a potential bug site.

---

## Specific Code Smells

### Keyboard Handling is 250-line If-Chain

```cpp
if (cmdOrCtrl) {
    if (key == GLFW_KEY_W) { ... }
    else if (key == GLFW_KEY_A) { ... }
    // 20+ branches
}
else if (shift) { ... }
else if (key == GLFW_KEY_LEFT) {
    if (cmdOrCtrl) { ... }
    else if (alt) { ... }
    // nested branches
}
```

No command abstraction. Can't rebind keys. Can't test in isolation.

### Undo System is Naive

```cpp
struct UndoState {
    std::string text;  // ENTIRE document copied!
    int cursorPos;
};
std::vector<UndoState> undoStack;  // MAX 100 states
```

**Problems:**
- 10MB document × 100 states = 1GB worst case
- Each character is separate undo state
- No redo
- No selection restoration

### Selection Logic Scattered

`hasSelection`, `selectionStart`, `selectionEnd` modified in:
- `handleKeyboard()` (shift selection, Cmd+A)
- `handleMouse()` (click, double-click, triple-click)
- `handleMouseMove()` (dragging)
- `moveCursor()`, `moveCursorByWord()`, `moveCursorVertically()`
- `insertChar()`, `deleteChar()`, `paste()`
- `undo()`

No centralized selection API.

### Toolbar Embedded in Engine

Toolbar rendering and hit testing hardcoded:
- Button positions as magic numbers
- Colors hardcoded
- Tight coupling with `applyBold()`, `applyLink()`, etc.

---

## Recommended Refactoring Direction

### Phase 1: Extract from Engine

| Extract To | Lines Removed | What |
|------------|---------------|------|
| `TextEditor` | ~400 | Text editing, cursor, selection, undo |
| `InputHandler` | ~250 | Keyboard dispatch, key bindings |
| `RawTextRenderer` | ~400 | Raw mode rendering, hit testing |
| `ToolbarUI` | ~150 | Toolbar rendering and interaction |
| `CaretAnimator` | ~50 | Blink state, animation lerp |
| `ScrollManager` | ~100 | Scroll offset, clamping, smooth scroll |

Engine becomes thin coordinator (~200 lines).

### Phase 2: Unify Position System

Choose ONE coordinate system:
- Option A: Raw positions everywhere (simpler, but layout needs raw→screen)
- Option B: DOM positions everywhere (cleaner, but editing needs DOM→raw)

Either way, eliminate constant conversion.

### Phase 3: Merge Rendering Pipelines

Raw mode should use same pipeline as markdown mode:
- Create `RawTextRenderer` implementing same interface as `MarkdownRenderer`
- Share: hit testing, cursor positioning, selection painting
- Differ only in: parsing (none) and styling (syntax colors)

### Phase 4: Cache Position Lookups

Instead of O(n) traversals:
```cpp
// Build once per layout
std::vector<LayoutObject*> layoutByDOMPosition;

// O(1) lookup
LayoutObject* getLayoutForPosition(int domPos) {
    return layoutByDOMPosition[domPos];
}
```

---

## One-Way Data Flow (Target Architecture)

```
User Input
    ↓
InputHandler (dispatches commands)
    ↓
TextEditor (modifies buffer + selection)
    ↓
MarkdownRenderer.setContent() [triggers reparse]
    ↓
Parse → Layout → Paint → Rasterize
    ↓
Screen

CaretAnimator ← reads selection state, outputs animated position
ScrollManager ← reads content height, outputs scroll offset
```

**Key principle:** Data flows DOWN. Components don't reach up to query state.

---

## Boundaries (Target)

```
┌─────────────────────────────────────────────────────────┐
│ UI Layer                                                │
│  - InputHandler (keyboard/mouse → commands)             │
│  - ToolbarUI (buttons → commands)                       │
│  - ScrollManager (scroll input → offset)                │
└─────────────────────────────────────────────────────────┘
                           ↓ commands
┌─────────────────────────────────────────────────────────┐
│ Editor Layer                                            │
│  - TextEditor (buffer, cursor, selection, undo)         │
│  - SelectionState (single source of truth)              │
└─────────────────────────────────────────────────────────┘
                           ↓ content + selection
┌─────────────────────────────────────────────────────────┐
│ Render Layer                                            │
│  - MarkdownRenderer OR RawTextRenderer                  │
│  - Parse → Layout → Paint → Rasterize                   │
└─────────────────────────────────────────────────────────┘
                           ↓ OpenGL
┌─────────────────────────────────────────────────────────┐
│ Platform Layer                                          │
│  - GLFW window, input callbacks                         │
│  - OpenGL context                                       │
└─────────────────────────────────────────────────────────┘
```

---

## Summary of Issues by Severity

| Severity | Issue | Impact |
|----------|-------|--------|
| **Critical** | Engine God Class (1800 lines) | Can't test, maintain, or extend |
| **Critical** | Dual coordinate systems | Conversion bugs, O(n) per frame |
| **Critical** | Dual rendering pipelines | Code duplication, divergence |
| **High** | State duplication (cursor in 2 places) | Sync bugs |
| **High** | O(n) tree traversals per keystroke | Perf on large docs |
| **High** | Selection logic scattered | Manual state management |
| **Medium** | Naive undo (full copies) | Memory, no redo |
| **Medium** | Keyboard 250-line if-chain | Can't rebind or test |
| **Medium** | Toolbar embedded in Engine | UI/logic mixing |
| **Low** | No command abstraction | Limited extensibility |

---

## Next Steps

1. **Audit raw↔DOM conversions** - Map every call site, understand the gaps
2. **Design position abstraction** - Single type with explicit conversions at boundaries
3. **Extract TextEditor** - First refactor, highest impact
4. **Merge raw mode** - Use same pipeline with different parser
5. **Add position cache** - Eliminate O(n) lookups

The goal: Engine.cpp shrinks from 1800 lines to ~200 lines of coordination code.
