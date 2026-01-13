# Document Model

## Purpose

The DocumentTree represents the semantic structure of markdown content. It answers the question: "What does this text *mean*?"

## Current Problems

The parser is a 516-line monolithic function with all detection logic inlined. Inline formatting exists in three competing representations: tree nodes (declared but never used), span annotations (the actual runtime model), and bitmask flags (for rendering). Six detection methods are stubbed out, returning false.

The result: formatting information is queried differently depending on who's asking, and adding new elements requires changes in multiple places.

## Target Model

### Complete Tree Structure

Every piece of text has exactly one path through the tree. Formatting is structural - bold text is a Strong node containing Text children, not a paragraph with "bold from position 5 to 9" annotations.

Example: "Hello **world**" becomes:
- Paragraph
  - Text: "Hello "
  - Strong
    - Text: "world"

Not:
- Paragraph with text "Hello world" and annotation {start: 6, length: 5, style: bold}

The tree representation means traversal is natural, nesting is explicit, and there's only one model to understand.

### Source Mapping

Every node knows where it came from in the source text: byte offset start and end. This enables:
- Cursor positioning without coordinate translation
- Hit-testing that returns source positions directly
- Selection in a single coordinate space

The current raw-to-DOM position translation (350+ lines) becomes unnecessary because there's only one position space: source bytes.

### Resolved at Parse Time

The parser resolves everything it can at parse time:
- Heading levels (1-6)
- List markers and numbering
- Code block languages
- Link URLs and titles
- Table cell alignments

Downstream consumers never need to re-parse or interpret markdown syntax. The tree contains the answers.

## Node Types

The tree contains nodes for:

**Block-level**: Document (root), Paragraph, Heading, BlockQuote, CodeBlock, List, ListItem, Table, TableRow, TableCell, ThematicBreak

**Inline-level**: Text (plain content), Emphasis, Strong, Code, Link, Image, LineBreak

Each node type carries its specific attributes (heading level, code language, link URL, etc.) rather than generic key-value metadata.

### Atomic Elements

Some nodes are **atomic**: they cannot be partially selected or have the caret inside them. Images are atomic - clicking an image selects the whole thing, not a position within it.

Atomic elements have source ranges like any other node, but hit-testing treats them as indivisible units.

## Copy Behavior

When copying a selection that spans formatting syntax, the clipboard receives valid markdown with intelligently closed syntax.

Example: Selecting "llo **wor" from "Hello **world**" produces "llo **wor**" in the clipboard - the bold syntax is properly closed.

This ensures pasted content is always valid markdown.

## What Gets Deleted

- The span annotation model for inline formatting
- The InlineLinkRange struct (links are tree nodes)
- The unused Bold/Italic/Underline enum values
- The six stubbed detection methods
- The domToRaw/rawToDOM translation functions

## Selection Boundaries

Double-click selects a "word" - but in visual mode, formatting syntax is part of the word.

Double-clicking "world" in rendered `**world**` selects the entire source range 0-9, including the asterisks. This ensures formatting isn't orphaned when deleting words.

Triple-click selects the current paragraph.

Cross-block selection is allowed - selecting from one paragraph into a list selects all content and syntax between.

## Success Criteria

The parser becomes a clean recursive descent structure. Detection functions are real and testable. One representation exists for inline formatting. Source positions flow naturally through the tree without translation.

See [06-edge-cases.md](06-edge-cases.md) for complete interaction rules.
