# Markdown Parser Specification

## 1. Purpose and Overview

The markdown parser is a core component of the promark markdown editor. It transforms raw markdown text into a Document Object Model (DOM) tree that can be processed by the layout engine for rendering.

**Key Responsibilities:**
- Parse markdown text into a hierarchical DOM structure
- Track source positions for bidirectional mapping between raw text and rendered output
- Extract inline formatting (bold, italic, code, links) with character-level precision
- Support a subset of markdown features suitable for a WYSIWYG-style editor

**Entry Points:**
```cpp
std::unique_ptr<MarkdownObject> parse(const TextBuffer& buffer);
std::unique_ptr<MarkdownObject> parse(const std::string& markdown);
```

Both methods delegate to `parseDocument()` which performs the actual parsing.

---

## 2. Markdown DOM Structure

### 2.1 Object Type Hierarchy

The DOM uses a class hierarchy rooted at `MarkdownObject`:

```
MarkdownObject (base)
├── HeadingObject      - ATX headings (# to ######)
├── ImageObject        - Images ![alt](src)
├── LinkObject         - Standalone links (unused in current impl)
├── BlockQuoteObject   - Block quotes (>)
├── ListObject         - Ordered/unordered lists
├── ListItemObject     - Individual list items
├── CodeBlockObject    - Fenced code blocks (```)
├── FrontmatterObject  - YAML frontmatter (---)
├── TableObject        - GFM tables
├── TableRowObject     - Table rows
├── TableCellObject    - Table cells
└── (Paragraph/Text)   - Plain MarkdownObject instances
```

### 2.2 Object Types Enumeration

```cpp
enum class MarkdownObjectType {
    Document,      // Root container
    Heading,       // # Heading
    Paragraph,     // Plain text paragraph
    Image,         // ![alt](src)
    Bold,          // **text** (unused - handled via style ranges)
    Italic,        // *text* (unused - handled via style ranges)
    Underline,     // Reserved
    Link,          // [text](url)
    BlockQuote,    // > quoted text
    CodeBlock,     // ```code```
    Frontmatter,   // ---yaml---
    Equation,      // Reserved for LaTeX math
    List,          // Container for list items
    ListItem,      // - item or 1. item
    Table,         // | table |
    TableRow,      // Row container
    TableCell,     // Cell container
    Text           // Leaf text node
};
```

### 2.3 Base Object Properties

Every `MarkdownObject` contains:

| Property | Type | Description |
|----------|------|-------------|
| `type` | `MarkdownObjectType` | Node type identifier |
| `children` | `vector<unique_ptr<MarkdownObject>>` | Child nodes |
| `text` | `string` | Display text (stripped of syntax) |
| `rawStart` | `int` | Start position in source markdown |
| `rawEnd` | `int` | End position in source markdown |
| `textOffset` | `int` | Offset from rawStart to visible text |
| `linkRanges` | `vector<InlineLinkRange>` | Inline link spans |
| `styleRanges` | `vector<InlineStyleRange>` | Bold/italic/code spans |

---

## 3. Parser Algorithm and Approach

### 3.1 Overall Strategy

The parser uses a **single-pass, line-oriented** approach:

1. Process the document line by line
2. Identify block-level elements by examining line prefixes
3. For each block, extract content and parse inline elements
4. Build DOM tree with parent-child relationships

### 3.2 Parsing Flow

```
Input: Raw markdown string
        ↓
┌─────────────────────────────┐
│  Check for Frontmatter      │  (must be at document start)
│  if starts with "---"       │
└─────────────────────────────┘
        ↓
┌─────────────────────────────┐
│  Main Loop: For each line   │
│  ├─ Empty line → Paragraph  │
│  ├─ ``` → Code Block        │
│  ├─ # → Heading             │
│  ├─ > → Block Quote         │
│  ├─ ![ → Image              │
│  ├─ -/* or 1. → List        │
│  ├─ | → Table               │
│  └─ else → Paragraph        │
└─────────────────────────────┘
        ↓
Output: Document (root MarkdownObject)
```

### 3.3 Block Detection Priority

Blocks are detected in this order (first match wins):

1. **Empty line** - Creates empty paragraph
2. **Code block** (```) - Consumes until closing ```
3. **Heading** (`#`) - Single line
4. **Block quote** (`>`) - Collects consecutive `>` lines
5. **Image** (`![`) - Single line
6. **List item** (`-`, `*`, `1.`, `a.`) - Collects consecutive items
7. **Table** (`|`) - Requires separator line validation
8. **Paragraph** - Default fallback

### 3.4 Multi-line Block Handling

Some blocks span multiple lines:

- **Block quotes**: Consecutive lines starting with `>` are merged
- **Code blocks**: Everything between ``` delimiters
- **Lists**: Consecutive list items (empty line terminates)
- **Tables**: Header + separator + body rows

---

## 4. Supported Markdown Features

### 4.1 Block Elements

| Feature | Syntax | Notes |
|---------|--------|-------|
| Headings | `# H1` to `###### H6` | ATX style only |
| Paragraphs | Plain text | Line-based |
| Code Blocks | ` ```lang ... ``` ` | Language identifier supported |
| Block Quotes | `> text` | Multi-line supported |
| Unordered Lists | `- item` or `* item` | Nested via indentation |
| Ordered Lists | `1. item`, `a. item` | Number, letter markers |
| Tables | `| col | col |` | GFM-style with alignment |
| Images | `![alt](src)` | Block-level only |
| Frontmatter | `--- ... ---` | At document start only |

### 4.2 Inline Elements

| Feature | Syntax | Style Flag |
|---------|--------|------------|
| Bold | `**text**` or `__text__` | `TextStyle::Bold` |
| Italic | `*text*` or `_text_` | `TextStyle::Italic` |
| Bold+Italic | `***text***` or `___text___` | `TextStyle::BoldItalic` |
| Inline Code | `` `code` `` | `TextStyle::Code` |
| Links | `[text](url)` | Via `InlineLinkRange` |
| Line Break | `<br>`, `<br/>`, `<br />` | Inserted as `\n` |

### 4.3 Table Alignment

Column alignment is determined from the separator line:

| Syntax | Alignment |
|--------|-----------|
| `---` or `:---` | Left (default) |
| `:---:` | Center |
| `---:` | Right |

### 4.4 List Marker Types

```cpp
enum class ListMarkerType {
    Bullet,     // - or *
    Number,     // 1. 2. 3.
    Letter      // a. b. c. or A. B. C.
};
```

---

## 5. Inline Formatting Handling

### 5.1 The parseInlineElements Function

Inline formatting is processed by `parseInlineElements()`, which:

1. Takes raw line text and parent object
2. Iterates character by character
3. Detects formatting markers
4. Builds display text (syntax removed)
5. Records style/link ranges on parent

```cpp
std::string parseInlineElements(const std::string& line,
                                 int lineRawStart,
                                 MarkdownObject* parent);
```

### 5.2 Detection Order

Inline elements are detected in this priority:

1. `***` or `___` - Bold+Italic (3 chars)
2. `**` or `__` - Bold (2 chars)
3. `*` or `_` - Italic (1 char)
4. `[` - Link start
5. `<br>` variants - HTML line break
6. `` ` `` - Inline code

### 5.3 Style Ranges

Styles are recorded as ranges on the parent object:

```cpp
struct InlineStyleRange {
    int startChar;  // Index in display text
    int endChar;    // Index in display text
    TextStyle style;
};
```

Example: For `Hello **world**`:
- Display text: `Hello world`
- Style range: `{6, 11, Bold}`

### 5.4 Link Ranges

Links are tracked separately:

```cpp
struct InlineLinkRange {
    int startChar;  // Index in display text
    int endChar;    // Index in display text
    std::string url;
};
```

Example: For `Click [here](https://example.com)`:
- Display text: `Click here`
- Link range: `{6, 10, "https://example.com"}`

### 5.5 Delimiter Matching

The `findClosingDelimiter()` helper finds matching closing delimiters:

```cpp
static size_t findClosingDelimiter(const std::string& line,
                                    size_t start,
                                    const std::string& delim);
```

Returns the position of the closing delimiter or `std::string::npos`.

---

## 6. Text Spans and Source Mapping

### 6.1 Raw Position Tracking

Every DOM node tracks its source position:

```cpp
void setRawRange(int start, int end);
int getRawStart() const;
int getRawEnd() const;
```

This enables:
- Cursor position mapping between raw text and rendered view
- Incremental parsing (future)
- Error highlighting

### 6.2 Text Offset

The `textOffset` property indicates where visible content begins within a node's raw range:

```cpp
void setTextOffset(int offset);
int getTextOffset() const;
```

For a heading `# Hello`:
- `rawStart = 0`
- `rawEnd = 7` (after newline)
- `textOffset = 2` (skip "# ")

### 6.3 DOM-to-Raw Mapping Example

For markdown: `# Hello **world**`

```
Document [0, 18]
└── Heading (level=1) [0, 18]
    └── Text [2, 17]
        text = "Hello world"
        styleRanges = [{6, 11, Bold}]
```

The layout engine uses these ranges to:
1. Map rendered positions back to raw positions
2. Apply cursor movement correctly
3. Highlight syntax in the raw view

---

## 7. Notable Implementation Details

### 7.1 Empty Lines as Paragraphs

Empty lines create empty paragraph nodes rather than being discarded. This preserves document structure and enables whitespace editing:

```cpp
if (line.empty()) {
    auto emptyParagraph = std::make_unique<MarkdownObject>(MarkdownObjectType::Paragraph);
    // ...
}
```

### 7.2 Frontmatter Parsing

Frontmatter must appear at document start. The parser uses `goto` for flow control:

```cpp
if (textLen >= 3 && text[0] == '-' && text[1] == '-' && text[2] == '-') {
    // Parse frontmatter...
    goto continue_parsing;
}
not_frontmatter:
continue_parsing:
```

Frontmatter validation:
- Opening `---` must be at position 0
- Only whitespace allowed after `---` on delimiter lines
- Content is everything between delimiters

### 7.3 Block Quote Line Merging

Consecutive `>` lines are combined into a single block quote with `\n` separators:

```cpp
if (!firstLine) {
    combinedText += '\n';
}
combinedText += lineText;
```

### 7.4 Table Parsing Requirements

A valid table requires:
1. First line starting with `|`
2. Second line being a valid separator (`|---|`)
3. Separator determines column count and alignment

Invalid tables fall back to paragraph parsing.

### 7.5 List Indentation

List items track indentation level based on leading whitespace:

```cpp
indent += (scanLine[contentStart] == '\t') ? 4 : 1;
listItem->setIndentLevel(indent / 2);  // 2 spaces per level
```

### 7.6 Inline Formatting in All Contexts

Inline elements are parsed in:
- Headings
- Paragraphs
- Block quotes
- List items
- Table cells

This is done by calling `parseInlineElements()` for each text-containing block.

### 7.7 Text Node Structure

Most block elements contain a child Text node:

```cpp
auto textNode = std::make_unique<MarkdownObject>(MarkdownObjectType::Text);
textNode->setText(displayText);
textNode->setRawRange(textRawStart, textRawEnd);
textNode->setTextOffset(0);
parent->addChild(std::move(textNode));
```

### 7.8 Memory Management

The parser uses `std::unique_ptr` throughout, ensuring:
- Automatic cleanup when DOM is destroyed
- Clear ownership semantics
- No memory leaks

### 7.9 Stub Methods

Several methods are declared but not implemented (marked TODO):
- `parseBlock()` - Unused
- `parseInline()` - Unused
- `isHeading()`, `isBlockQuote()`, `isCodeBlock()`, `isList()` - Unused

The main parsing logic is in `parseDocument()` which handles all detection inline.

---

## 8. TextStyle Flags

Text styles use bit flags for combination:

```cpp
enum class TextStyle : uint8_t {
    Normal = 0,
    Bold = 1 << 0,
    Italic = 1 << 1,
    Code = 1 << 2,
    BoldItalic = Bold | Italic
};
```

Helper functions:
```cpp
TextStyle operator|(TextStyle a, TextStyle b);  // Combine styles
bool hasStyle(TextStyle style, TextStyle flag); // Check for style
```

---

## 9. File Locations

| File | Purpose |
|------|---------|
| `src/engine/markdown_parser.h` | Parser class declaration |
| `src/engine/markdown_parser.cpp` | Parser implementation |
| `src/engine/markdown_objects.h` | DOM object declarations |
| `src/engine/markdown_objects.cpp` | DOM object implementations |
