# Document Model: Synthesized Analysis

**Source files:** `markdown_objects.h/cpp`, `markdown_parser.h/cpp`

---

## Core Architecture

The document model is a two-layer system:
1. **DOM layer** (`markdown_objects`) - type hierarchy and tree structure
2. **Parser layer** (`markdown_parser`) - single-pass, line-oriented construction

Data flows: raw text -> parser -> DOM tree -> layout engine -> painter

---

## Cross-Cutting Concerns

### 1. Inline Formatting Authority Split

The fundamental tension: **who owns inline formatting?**

| Approach | Where Declared | Where Used |
|----------|---------------|------------|
| Tree nodes | `MarkdownObjectType::Bold/Italic/Underline` enum | Never instantiated |
| Span annotations | `InlineLinkRange`, `InlineStyleRange` structs | Actual runtime model |
| Bitmask flags | `TextStyle` enum (Bold=1, Italic=2, Code=4) | Rendering layer |

**Result:** Three parallel representations of the same concept. The parser populates span annotations on *parent* block nodes (paragraphs, headings), while child Text nodes have no knowledge of their own formatting. Layout retrieves ranges via `getStyleRanges()` from the source `MarkdownObject`, bypassing the tree structure entirely.

### 2. Link Representation Dualism

Links exist as both:
- `LinkObject` class (tree node with URL)
- `InlineLinkRange` struct (span annotation on parent)

No clear rule governs which to use. Evidence suggests `InlineLinkRange` is the runtime model, `LinkObject` is vestigial.

### 3. Mutability Without Lifecycle

Both files acknowledge a construction-then-immutable pattern, but neither enforces it:
- All DOM setters are public
- Parser is stateless but DOM is infinitely mutable
- `LayoutObject` uses `const MarkdownObject*` (runtime protection only)

No freeze mechanism exists.

---

## Unstable Boundaries

### Parser-Object Coupling

The parser has **deep structural knowledge** of every object type, directly calling:
- `HeadingObject::HeadingObject(level)`
- `CodeBlockObject::setCode()`
- `ListItemObject::setMarkerType/setMarkerText/setIndentLevel()`
- `TableObject::setColumnAlignments()`

Any object schema change forces parser modification. No factory abstraction.

### TextBuffer: Pass-Through Dependency

```cpp
std::unique_ptr<MarkdownObject> MarkdownParser::parse(const TextBuffer& buffer) {
    return parse(buffer.getText());  // Immediately discards abstraction
}
```

The dependency exists but provides no value - intended for incremental parsing, currently just `const std::string&` with extra steps.

---

## Paper Abstractions

### Declared But Never Implemented

| Declaration | Status | Evidence |
|-------------|--------|----------|
| `parseBlock(text, position)` | Returns nullptr | "TODO: Implement" |
| `parseInline(text, position)` | Returns nullptr | "TODO: Implement" |
| `isHeading(text, position)` | Returns false | Inline check used instead |
| `isBlockQuote(text, position)` | Returns false | Inline check used instead |
| `isCodeBlock(text, position)` | Returns false | Inline check used instead |
| `isList(text, position)` | Returns false | `isListItem(line)` used |

**Intended design:** Recursive descent parser with modular detection predicates.
**Actual implementation:** 516-line monolithic `parseDocument()` with inline detection.

### Asymmetric Detection APIs

Two list detection patterns coexist:
- Stubbed: `isList(const std::string& text, size_t position)` - unused
- Implemented: `isListItem(const std::string& line)` - different signature

The implemented version takes a line, the stubbed version takes text+position. Design was abandoned mid-refactor.

### findClosingDelimiter Dead Code

```cpp
while (pos < line.length()) {
    size_t found = line.find(delim, pos);
    // ... escape handling comment
    return found;  // Returns immediately, loop never iterates
}
```

The comment acknowledges escape handling should exist; the implementation returns on first match.

---

## Inconsistent Content Storage

| Object Type | Content Field | Why Different? |
|-------------|--------------|----------------|
| Base `MarkdownObject` | `text` | Standard |
| `CodeBlockObject` | `code` | Separate member |
| `FrontmatterObject` | `content` | Separate member |

Suggests base `text` field was added after subclasses, or subclasses predate unified model.

---

## Denormalized State

**Table alignments** stored in two places:
1. `TableObject::columnAlignments` - vector for whole table
2. `TableCellObject::alignment` - per-cell copy

Sync burden exists between parent table and child cells.

---

## Control Flow Violations

Frontmatter parsing uses `goto` for non-local jumps:
```cpp
goto not_frontmatter;   // escape nested validation
goto continue_parsing;  // skip to main loop
not_frontmatter:
continue_parsing:
```

HTML `<br>` parsing embedded in markdown parser - incomplete (only handles `<br>`, no other HTML).

---

## Architectural Debt Summary

| Issue | Severity | Impact |
|-------|----------|--------|
| Three inline formatting models | High | Confusion, maintenance burden |
| Stubbed interface, monolithic reality | High | 516-line method, technical lie in API |
| No immutability enforcement | Medium | Potential mutation bugs |
| TextBuffer pass-through | Low | Useless abstraction |
| Table alignment denormalization | Low | Sync bugs possible |
| Unused includes (`sstream`, `iostream`) | Trivial | Dead code |

---

## Recommended Unification

1. **Pick one inline model** - either tree nodes or span annotations, not both
2. **Remove or implement** stubbed detection methods
3. **Strengthen or remove** TextBuffer dependency
4. **Add construction freeze** - builder pattern or immutable-after-build enforcement
5. **Extract factory** for parser-object decoupling
