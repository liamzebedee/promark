# Bug: caret-column-heading-to-paragraph

## Reproduction Steps

1. Load `resources/long-post.md` into the editor
2. Navigate to line 6: `## Peace and work.`
3. Place caret at the "P" in "Peace" (visually the first letter of the heading text)
4. Press Down arrow to move to line 8: `One of the ways to approach life...`
5. Observe where the caret lands
6. Press Up arrow to return to the heading
7. Observe where the caret lands

## Expected Behavior

- When pressing Down from "P" in "Peace", caret should land at "O" in "One" (same visual column)
- When pressing Up from "O" in "One", caret should return to "P" in "Peace"
- Column memory should be preserved across up/down navigation
- Visual column position should be maintained, not raw character offset

## Actual Behavior

- Caret behaves weirdly around column position when navigating between heading and paragraph
- (Document specific behavior: does it jump to wrong column? Does it account for `## ` prefix incorrectly?)

## Possible Causes

- Heading prefix `## ` may be hidden in visual mode but counted in column calculation
- Different rendering/layout between heading and paragraph blocks affecting column mapping
- Column memory using raw character position instead of visual x-coordinate
- Block-level vs inline-level position translation issue

## Related Test Cases

- Navigation between any heading level (`#`, `##`, `###`) and paragraph
- Navigation between blockquote (`> `) and regular paragraph
- Navigation between list items (`- `, `1. `) and regular text

## Severity

Medium - affects navigation usability in documents with mixed block types
