---
name: glfw-visual-loop
description: >
  Visual feedback loop for native C++/GLFW apps. Use this skill when working on a
  GLFW-based C++ application that renders to a window and you need to verify visual changes
  actually worked. Trigger when making UI or rendering changes and need to confirm they look
  correct, when user asks to verify something visual like "check if the font renders correctly",
  when debugging visual issues, or any time you need to "see" what the app currently looks like.
  This skill builds with make, launches the app, captures a screenshot, and returns it for
  visual inspection.
---

# GLFW Visual Loop

Visual feedback loop for C++/GLFW apps using headless rendering.

## Preferred: Headless Test Framework

The project includes a headless visual test framework in `protos/a/tests/` that renders offscreen without opening a window. This is faster and more reliable than screen capture.

### Run all visual tests

```bash
cd protos/a && make test
```

This builds and runs all tests, outputting:
- `PASS: TestName` or `FAIL: TestName - reason`
- `SCREENSHOT: /path/to/file.png` for each captured frame

### Run a specific test

```bash
cd protos/a && make test
./build/run_tests --test BasicFormatting
```

Available tests:
- `BasicFormatting` - Headers, bold, italic, links, strikethrough
- `InlineBlock` - Inline code, fenced code blocks, indented code
- `BlockLayout` - Lists, blockquotes, horizontal rules, tables, scroll

### Custom output directory

```bash
TEST_OUTPUT_DIR=/tmp/my_tests ./build/run_tests
```

### View screenshots

Screenshots are saved to `/tmp/promark_tests/` by default. Read them to inspect:

```bash
# After running tests, view specific screenshot
cat /tmp/promark_tests/basic_formatting_headers.png
```

## Adding new tests

Create a new test file in `protos/a/tests/`:

```cpp
#include "test_helpers.h"

TestResult testMyFeature(TestContext& ctx) {
    std::vector<std::string> screenshots;

    // Set content and capture
    ctx.setContent("# My Test\n\nSome **markdown** content.");
    screenshots.push_back(ctx.captureScreenshot("my_feature_test"));

    // Simulate interactions
    ctx.simulateScroll(-5);  // Scroll down
    screenshots.push_back(ctx.captureScreenshot("my_feature_scrolled", 1));

    TEST_PASS();
}

REGISTER_TEST(MyFeature, testMyFeature);
```

Then add to `Makefile` in `TEST_SOURCES`.

## Fallback: Screen Capture (Legacy)

For cases where headless doesn't work, use screen capture:

```bash
pkill -f mdeditor 2>/dev/null
cd protos/a && make && ./build/mdeditor &
sleep 2
screencapture -x /tmp/capture.png  # macOS
# or: import -window root /tmp/capture.png  # Linux with ImageMagick
pkill -f mdeditor 2>/dev/null
```

Then read `/tmp/capture.png` to inspect.

## After capture

View the screenshot image to verify visual correctness. Compare against expected appearance and report findings to user.
