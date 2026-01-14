# Bug: font-typography-readability

## Reproduction Steps

1. Load `resources/long-post.md` into the editor
2. Compare the rendered text visually against the same markdown rendered in a browser (e.g., GitHub, VS Code preview, or a markdown viewer)
3. Note differences in text clarity and readability

## Expected Behavior

Text should be as clear and readable as browser-rendered text:
- Crisp, well-hinted font rendering
- Proper subpixel anti-aliasing (or grayscale AA matching system settings)
- Comfortable line height (~1.5-1.6x font size for body text)
- Appropriate letter-spacing (not too tight, not too loose)
- Correct font weight (not too thin/light)
- Proper contrast between text and background
- Consistent baseline alignment

## Actual Behavior

Text appears harder to read than browser equivalent. Potential causes to investigate:

- **Font hinting**: May be missing or incorrect hinting, causing blurry/fuzzy edges
- **Anti-aliasing mode**: May be using wrong AA mode for the platform (LCD vs grayscale)
- **Line height**: May be too tight, making lines feel cramped
- **Letter-spacing**: May be too tight or inconsistent
- **Font weight**: May be rendering too light/thin
- **Font choice**: The font itself may not be optimized for screen rendering at this size
- **DPI/scaling**: May not be accounting for display scaling correctly
- **Gamma/contrast**: Text-background contrast may be off

## Investigation Checklist

- [ ] Compare font rendering settings against system defaults
- [ ] Check if font hinting is enabled
- [ ] Verify anti-aliasing mode matches platform expectations
- [ ] Measure line-height ratio (should be ~1.5x for body text)
- [ ] Check letter-spacing values
- [ ] Verify font weight being used (400 normal, not 300 light)
- [ ] Test with different fonts to isolate font vs rendering issue
- [ ] Check HiDPI/Retina rendering path
- [ ] Compare RGB subpixel order if using LCD anti-aliasing

## Severity

Medium - affects reading comfort and long-form editing experience
