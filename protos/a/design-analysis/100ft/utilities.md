# Utilities Layer: Architectural Synthesis

**Source files:** `text_buffer.{h,cpp}`, `clipboard.{h,cpp}`, `utf8.h`, `gl_includes.h`, `shaders_embedded.h`, `typography.h`

---

## Pattern 1: Wrapper Abstractions That Don't Abstract

**TextBuffer** wraps `std::string` but adds no value. The API (`insertText`/`deleteText`) exists but is never called. Engine mutates a raw `char[10MB]` buffer, reconstructs a string, and copies wholesale via `setText()`. Three copies exist: `char* inputBuffer` (authoritative), `Engine::textBuffer` (secondary), `MarkdownRenderer::textBuffer` (tertiary). Synchronization happens 18+ times in engine.cpp via copy-construct-transfer pattern.

**Clipboard** claims "platform-agnostic" but hard-includes GLFW unconditionally. The runtime callback mechanism exists but compile-time dependency remains. Web builds must stub GLFW headers.

**Pattern:** Both modules declare abstraction boundaries that don't hold. The abstraction is structural (exists in types/interfaces) but not mechanical (behavior bypasses the interface).

---

## Pattern 2: Silent Failure Modes

All three domains exhibit silent failure:

| Module | Failure | Returns |
|--------|---------|---------|
| TextBuffer | API not used | N/A - never exercised |
| Clipboard | No GLFW context | Empty string or silent no-op |
| UTF-8 decode | Invalid sequence | Raw byte, advance 1 |

Callers cannot distinguish success-with-empty-data from failure. No error callbacks, exceptions, or return codes.

---

## Pattern 3: Static Global State Without Safety

**Clipboard** uses static members (`s_getText`, `s_setText`, `s_useCustom`) with no synchronization. Handler mutation during operation is UB. No lifecycle/ownership model - any code can call `setHandlers()` at any time.

**Implicit global state:** Clipboard reaches into GLFW via `glfwGetCurrentContext()` - hidden dependency on GL context thread.

---

## Pattern 4: Asymmetric APIs

| API | Asymmetry |
|-----|-----------|
| UTF-8 | Decode only, no encode |
| Clipboard handlers | Partial set accepted, then ignored (both or neither) |
| Shaders | IMAGE_FRAG exists, IMAGE_VERT missing (implicit pairing with TEXT_VERT) |
| Typography | Sizes defined, no weight/style constants |

Each asymmetry implies workaround code elsewhere. UTF-8 encode must exist somewhere for clipboard/file operations.

---

## Pattern 5: Platform Claims vs. Reality

**gl_includes.h:**
- Claims "OpenGL ES 2.0 style"
- macOS path includes desktop GL (`<OpenGL/gl.h>`)
- No Windows support (falls into Linux branch)
- No runtime capability checking

**shaders_embedded.h:**
- No `#version` directive (locked to GLSL 1.10/ESSL 1.00)
- No precision qualifiers (may fail strict GLES2)
- Hardcoded attribute names with no shared constants

**typography.h:**
- All sizes in pixels, no DPI awareness
- `LINE_HEIGHT_RATIO = 1.0` (lines touch vertically)

---

## Unstable Boundaries

### Cursor State Orphaned
TextBuffer stores text but cursor/selection state lives in Engine (`cursorPos`, `selectionStart`, `selectionEnd`, `hasSelection`). These semantically belong together but are split across modules.

### Ownership Transfer Disguised as Value Copy
```cpp
markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
```
`unique_ptr` signature implies ownership semantics. Actual pattern is value-copy with ceremonial ownership transfer on every keystroke.

### Typography Constants Unlinked
`SCALE_RATIO = 1.2` is defined but heading sizes are hardcoded approximations. Spacing uses "rem equivalent" language but values don't scale with base font size.

---

## Paper Abstractions (Structure Without Mechanics)

1. **TextBuffer::insertText/deleteText** - Implemented, never called. Engine uses `memmove`/`memcpy` on raw buffer.

2. **Clipboard::hasCustomHandlers()** - Exists for callers to query handler state, but proper abstraction shouldn't require this. Presence indicates leaky abstraction.

3. **UTF-8 BOM handling** - Not implemented. File loading must handle separately.

4. **Shader auto-generation** - Comment claims shaders are generated from `.vert`/`.frag` files but no pipeline is evident. Manual edits risk being overwritten.

5. **Typography power-law** - Comment describes `BASE * SCALE_RATIO^(6-level)` relationship. Values are pre-computed, so changing `SCALE_RATIO` requires manual recalculation of all `H*_SIZE` constants.

---

## Cross-Cutting Issues

### Missing Utilities (Implied by Gaps)
- UTF-8 encoder
- DPI/scale factor conversion
- Shader compilation wrapper
- Attribute location constants (shader-vertex coupling)

### Dependency Direction

```
[Engine] --copies--> [TextBuffer] --copies--> [MarkdownRenderer::TextBuffer]
    |
    +--owns--> char[10MB] inputBuffer (true authority)
```

TextBuffer should be authoritative but isn't. The abstraction inverts: rather than Engine using TextBuffer's API, Engine manages raw memory and TextBuffer is a snapshot.

### Thread Safety Absent
Clipboard modifies static state without locks. TextBuffer copy pattern isn't thread-safe if parsing happens on background thread.

---

## Summary Table

| Concern | TextBuffer | Clipboard | Utilities |
|---------|------------|-----------|-----------|
| Dead API | insertText/deleteText | hasCustomHandlers | - |
| Silent Failure | N/A | Empty string on failure | Invalid UTF-8 returns byte |
| Platform Mismatch | - | GLFW always required | No Windows, ES2/GL mismatch |
| Authority | char* is authoritative | OS clipboard untracked | Constants not computed |
| Thread Safety | Copy-on-mutation | No synchronization | Pure functions (safe) |
