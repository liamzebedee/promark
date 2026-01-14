# Bug: Scrolling Down Shows None of Document in Visual Mode

## Reproduction Steps

1. Open a document that is longer than the visible viewport
2. Scroll down using scroll controls or mouse wheel
3. Observe the rendered content in visual mode

## Expected Behavior

The rest of the document should be visible when scrolling down. Content below the initial viewport should render and display as the user scrolls.

## Actual Behavior

None of the rest of the document is shown when scrolling down in visual mode. The content below the initial viewport does not appear.

## Severity

High - Users cannot view or interact with content beyond the initial viewport, making the editor unusable for documents longer than one screen.
