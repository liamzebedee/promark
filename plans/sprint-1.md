# Sprint 1: Markdown Editor Visual Milestones

## Sprint Goal
Build a working markdown word processor where you can see live markdown editing happen in real-time.

## Visual Editing Milestones

### 1. **See markdown text rendered** - Basic Document Display
- [ ] **See a heading** - Type `# Hello World` and see it render as large text
- [ ] **See multiple heading sizes** - Type `# Big`, `## Medium`, `### Small` and see size differences
- [ ] **See paragraphs** - Type multiple lines separated by blank lines, see paragraph spacing

### 2. **See text formatting** - Inline Formatting
- [ ] **See bold text** - Type `**bold**` and see it render in bold font
- [ ] **See italic text** - Type `*italic*` and see it render in italic font
- [ ] **See mixed formatting** - Type `**bold** and *italic* text` and see both styles

### 3. **See links** - Link Rendering
- [ ] **See a clickable link** - Type `[Google](https://google.com)` and see blue underlined text
- [ ] **See link hover effect** - Hover over link and see cursor change/highlight
- [ ] **See inline links** - Links embedded within paragraphs render correctly

### 4. **See lists** - List Formatting  
- [ ] **See bullet points** - Type `- Item 1\n- Item 2` and see bulleted list
- [ ] **See numbered lists** - Type `1. First\n2. Second` and see numbered list
- [ ] **See nested lists** - Type indented list items and see proper nesting

### 5. **See code** - Code Block Rendering
- [ ] **See inline code** - Type backticks around text: `code` and see monospace highlight
- [ ] **See code blocks** - Type triple backticks and see formatted code block
- [ ] **See code with syntax** - Type ```javascript and see language-specific highlighting

### 6. **See quotes** - Block Quote Rendering
- [ ] **See quoted text** - Type `> This is a quote` and see indented/styled quote block
- [ ] **See multi-line quotes** - Multiple `>` lines render as single quote block

### 7. **See live editing** - Real-time Updates
- [ ] **See instant updates** - Type `# ` and immediately see text become large (heading)
- [ ] **See formatting appear** - Type `**` around text and see bold appear instantly
- [ ] **See formatting disappear** - Delete markdown syntax and see formatting removed

### 8. **See cursor in formatted text** - Advanced Editing
- [ ] **See cursor in headings** - Click inside `# Heading` text and see cursor position
- [ ] **See cursor in bold text** - Click inside `**bold**` and edit the bold text
- [ ] **See cursor navigate correctly** - Arrow keys move logically through formatted text

### 9. **See document scrolling** - Long Document Handling
- [ ] **See long document** - Create 20+ headings/paragraphs that exceed screen height
- [ ] **See smooth scrolling** - Scroll wheel moves through document smoothly
- [ ] **See cursor stays visible** - Typing near bottom auto-scrolls to keep cursor visible

## Success Criteria
- Single HTML file loads a canvas
- Can type plain text and see it rendered via WebGL
- Can scroll through document using mouse wheel
- Can navigate with arrow keys and edit text
- Supports basic markdown: plain text, headers, paragraphs

## Out of Scope for Sprint 1
- Bold/italic/formatting
- Images
- Links
- Lists
- Code blocks
- Complex fonts/typography
- Selection highlighting
- Copy/paste
- File loading/saving

## Technical Constraints
- Single canvas with one WebGL context
- No DOM manipulation except hidden textarea
- Camera-based scrolling (no DOM scroll)
- Minimal file structure