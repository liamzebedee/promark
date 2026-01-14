# Bug: caret-highlights-in-blockquotes

## Reproduction Steps

1. Load `resources/long-post.md` into the editor
2. Navigate cursor into a blockquote section (e.g., lines 18-20, 48-50, or 96-100 which contain `>` prefixed text)
3. Observe the caret visibility
4. Select text within the blockquote by shift+arrow keys or click-drag
5. Observe the selection highlight visibility

## Expected Behavior

- The caret (blinking cursor) should be visible when positioned inside blockquote blocks
- Text selection highlights should be visible when selecting text within blockquotes
- Visual feedback should be consistent with how caret/highlights appear in regular paragraph text

## Actual Behavior

**Before fix:**
- Caret did not appear when cursor was inside a blockquote
- Selection highlights did not show when selecting text within blockquotes
- Cursor positioning worked correctly (text could be typed and selected), but visual feedback was missing

**Root Cause:**
The rasterizer was drawing display items in incorrect z-order:
1. Selection and caret (from root artifact's displayItems) were drawn first
2. Then children (content tree including blockquote backgrounds) were drawn on top

This caused blockquote backgrounds (Color 250,250,250,255 - nearly white) to cover both selection highlights and the caret.

**Fix:**
Changed rasterizer to draw items in correct z-order:
1. Non-selection, non-caret items from artifact
2. Recursively process children (content including backgrounds)
3. Selection (after backgrounds, visible on top)
4. Caret (last, on top of everything)

## Severity

High - users cannot see where they are typing or what they have selected inside blockquotes, making editing quote content very difficult
