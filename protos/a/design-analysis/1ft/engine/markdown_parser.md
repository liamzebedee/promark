# Design Analysis: markdown_parser

**Files:** `src/engine/markdown_parser.h`, `src/engine/markdown_parser.cpp`

---

## 1. Responsibilities

The `MarkdownParser` module is responsible for:

1. **Converting raw markdown text into a hierarchical AST** (`MarkdownObject` tree)
2. **Mapping raw character positions** to DOM nodes via `setRawRange()` calls
3. **Parsing inline formatting** (bold, italic, links, inline code) within block elements
4. **Recognizing block-level constructs**: headings, blockquotes, code blocks, lists, tables, images, frontmatter

The parser operates as a **single-pass, line-oriented parser** that processes input sequentially, building the document tree as it goes.

---

## 2. Dependencies

### Direct Dependencies

| Dependency | Header | Purpose |
|------------|--------|---------|
| `markdown_objects.h` | Line 2 (.h) | Defines `MarkdownObject` and all specialized subtypes (`HeadingObject`, `ListObject`, etc.) |
| `text_buffer.h` | Line 3 (.h) | Provides `TextBuffer` abstraction for input text |
| `<memory>` | Line 4 (.h) | `std::unique_ptr` for AST node ownership |
| `<sstream>` | Line 2 (.cpp) | Included but **unused** |
| `<iostream>` | Line 3 (.cpp) | Included but **unused** (likely debug remnant) |

### Dependency Analysis

**text_buffer.h dependency (lines 11-13, .cpp):**
```cpp
std::unique_ptr<MarkdownObject> MarkdownParser::parse(const TextBuffer& buffer) {
    return parse(buffer.getText());
}
```
The `TextBuffer` dependency is **trivial** - the parser immediately extracts the string and discards the buffer reference. This suggests:
- The abstraction was intended to be richer (incremental parsing, change notifications)
- Currently serves no purpose beyond `const std::string&`

**markdown_objects.h coupling:**
The parser has **deep knowledge** of the object hierarchy, directly constructing:
- `MarkdownObject` (lines 160, 254, 257, 321, 357, 414, 420, 450, 456, 512, 643, 648, 657, 662, 854, 866)
- `HeadingObject` (line 351)
- `BlockQuoteObject` (line 411)
- `CodeBlockObject` (line 316)
- `FrontmatterObject` (line 217)
- `ImageObject` (line 442)
- `ListObject` (line 465)
- `ListItemObject` (line 503)
- `TableObject` (line 603)
- `TableRowObject` (lines 608, 629)
- `TableCellObject` (lines 849, 865)

This tight coupling means any change to object construction (new required fields, validation) forces parser changes.

---

## 3. Mutation Points

### State Mutated by Parser

The parser is **stateless** between calls - it holds no instance state. All mutation occurs on:

1. **Output AST nodes** - Parser directly mutates newly-created `MarkdownObject` instances:
   - `setRawRange()` - raw position tracking
   - `setText()` - display text content
   - `setTextOffset()` - syntax offset
   - `addChild()` - tree structure
   - `addLinkRange()` - inline link metadata (line 111)
   - `addStyleRange()` - inline style metadata (lines 56, 74, 90, 146)

2. **Specialized object properties**:
   - `CodeBlockObject::setCode()` (line 317)
   - `FrontmatterObject::setContent()` (line 218)
   - `ListItemObject::setMarkerType/setMarkerText/setIndentLevel()` (lines 504-506)
   - `TableObject::setColumnAlignments()` (line 604)

### Authority Concerns

**Inline style/link ranges are stored on parent objects:**
```cpp
// Line 37-38, parseInlineElements signature
std::string MarkdownParser::parseInlineElements(const std::string& line, int lineRawStart,
                                                 MarkdownObject* parent) {
```

The parser modifies `parent` by calling `addLinkRange()` and `addStyleRange()` on it. This means:
- Inline formatting metadata lives on block-level nodes (paragraphs, headings)
- The `Text` child node has no knowledge of its own formatting
- Authority for "what formatting applies to what text" is split between parent ranges and child text content

**Line 39 reveals uncertainty:**
```cpp
(void)lineRawStart;  // Not used currently
```
The `lineRawStart` parameter exists in the signature but is explicitly suppressed - indicating incomplete implementation or abandoned design direction.

---

## 4. Boundary Violations

### Goto Usage (lines 173, 230, 238-239)

```cpp
goto not_frontmatter;  // line 173
goto continue_parsing; // line 230
not_frontmatter:       // line 238
continue_parsing:      // line 239
```

This control flow pattern violates structured programming expectations. The frontmatter parsing uses `goto` to escape nested loops, creating non-local jumps that make reasoning about state difficult.

### HTML Parsing in Markdown Parser (lines 119-136)

```cpp
// Check for <br> tag (HTML line break)
if (line[pos] == '<' && pos + 3 < line.length()) {
    std::string remaining = line.substr(pos);
    if (remaining.substr(0, 4) == "<br>" ||
        remaining.substr(0, 5) == "<br/>" ||
        remaining.substr(0, 6) == "<br />") {
```

The parser handles HTML `<br>` tags inline. This:
- Mixes HTML parsing concerns into the markdown parser
- Is incomplete (only handles `<br>`, no other HTML)
- Should either be comprehensive HTML-in-markdown handling or delegated

### Direct Type Construction in Parser

The parser directly instantiates concrete types rather than using a factory:
```cpp
auto heading = std::make_unique<HeadingObject>(level);      // line 351
auto blockquote = std::make_unique<BlockQuoteObject>();     // line 411
auto codeBlock = std::make_unique<CodeBlockObject>(language); // line 316
```

This prevents:
- Custom object pools/allocators
- Instrumented/debug object variants
- Alternative AST representations

---

## 5. Declared-but-Unrealised Design

### Stubbed Methods (lines 677-705)

The header declares a structured parsing API that is completely unused:

```cpp
// Header (lines 16-17)
std::unique_ptr<MarkdownObject> parseBlock(const std::string& text, size_t& position);
std::unique_ptr<MarkdownObject> parseInline(const std::string& text, size_t& position);

// Implementation (lines 677-685)
std::unique_ptr<MarkdownObject> MarkdownParser::parseBlock(const std::string& text, size_t& position) {
    // TODO: Implement block parsing
    return nullptr;
}

std::unique_ptr<MarkdownObject> MarkdownParser::parseInline(const std::string& text, size_t& position) {
    // TODO: Implement inline parsing
    return nullptr;
}
```

These suggest an intended **recursive descent parser** architecture where:
- `parseDocument` would delegate to `parseBlock`
- `parseBlock` would recognize block types and delegate to `parseInline`
- Each method would advance `position` and return a subtree

**Reality:** All parsing happens in the monolithic `parseDocument()` method (lines 159-675, 516 lines), with inline parsing delegated to `parseInlineElements()` (lines 37-157).

### Stubbed Detection Methods (lines 687-705)

```cpp
bool MarkdownParser::isHeading(const std::string& text, size_t position) {
    // TODO: Implement heading detection
    return false;
}

bool MarkdownParser::isBlockQuote(const std::string& text, size_t position) {
    // TODO: Implement blockquote detection
    return false;
}

bool MarkdownParser::isCodeBlock(const std::string& text, size_t position) {
    // TODO: Implement code block detection
    return false;
}

bool MarkdownParser::isList(const std::string& text, size_t position) {
    // TODO: Implement list detection
    return false;
}
```

These methods:
- Are declared in the header (lines 28-31)
- Return hardcoded `false`
- Are **never called** from `parseDocument()`

**Workaround:** Detection is done inline within `parseDocument()`:
- Heading detection: `if (line[0] == '#')` (line 333)
- Blockquote detection: `else if (line[0] == '>')` (line 364)
- Code block detection: `if (line.length() >= 3 && line[0] == '`' ...)` (line 269)
- List detection: `else if (isListItem(line))` (line 463) - uses different method

### Asymmetric List Detection

Two list detection patterns exist:
1. **Declared but stubbed:** `isList(text, position)` - returns false, unused
2. **Actually implemented:** `isListItem(line)` (lines 707-745), `isOrderedListItem(line)` (lines 747-770)

The implemented versions take a `line` string, not `(text, position)` like the stubbed interface. This suggests the design was abandoned mid-implementation.

### findClosingDelimiter Helper (lines 21-33)

```cpp
static size_t findClosingDelimiter(const std::string& line, size_t start, const std::string& delim) {
    size_t pos = start;
    while (pos < line.length()) {
        size_t found = line.find(delim, pos);
        if (found == std::string::npos) {
            return std::string::npos;
        }
        // Make sure it's not escaped and not at word boundary issues
        // For simplicity, just find the delimiter
        return found;  // <-- Returns immediately after first find
    }
    return std::string::npos;
}
```

The comment on line 28-29 acknowledges that escape handling and word boundary checking should happen, but the implementation **immediately returns** on first match. The while loop is effectively dead code after `return found`.

### Unused Includes

```cpp
#include <sstream>   // line 2, .cpp - never used
#include <iostream>  // line 3, .cpp - never used
```

These suggest removed debug code or planned string stream operations that were never implemented.

---

## Summary

The `MarkdownParser` exhibits a common evolutionary pattern: a clean interface was declared (`parseBlock`/`parseInline` recursion, detection methods) but implementation pressure led to a monolithic `parseDocument()` that handles everything inline. The TextBuffer abstraction provides no value, the detection methods are dead code, and inline formatting creates split authority between parent ranges and child text nodes.

Key architectural debts:
1. **516-line monolithic method** needs decomposition to match declared interface
2. **Stubbed methods** should be removed or implemented
3. **TextBuffer dependency** should be strengthened or removed
4. **Inline style authority** should be reconsidered (parent ranges vs. child ownership)
5. **goto control flow** in frontmatter parsing should be restructured
