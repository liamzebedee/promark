# Bug: Link Inside Bold Text Not Rendering

## Summary
When a markdown link is placed inside bold text, the link syntax is shown literally instead of being rendered as a clickable link.

## Reproduction

Input markdown:
```markdown
**Bold [link](https://bold.com)** inside bold text.
```

Expected rendering:
- "Bold " in bold
- "link" as a blue underlined clickable link (also possibly bold)
- " inside bold text." in normal weight

Actual rendering:
- "Bold" in bold
- `[link](https://bold.com)` shown literally as text
- " inside bold text." in normal weight

## Evidence
Screenshot from test_basic_formatting.cpp shows the raw `[link](url)` syntax visible in visual mode.

## Notes
- Standalone links render correctly
- Bold text without links renders correctly
- The parser code claims to support nested inline formatting
- This is a regression or parsing limitation
