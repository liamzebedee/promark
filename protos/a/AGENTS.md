# Agent Instructions

## Bug Fixing Protocol

When the user reports a bug during interaction, always write a visual unit test that:
1. Reproduces the bug (test should fail or show incorrect behavior before fix)
2. Verifies the fix works (test passes after fix)
3. Prevents regression (test remains in the suite)

This ensures bugs are verifiably solved and don't return.

## Visual Testing

Use the headless test framework to verify visual changes without opening windows.

### Quick verification

```bash
make test
```

Runs all visual tests, outputs `SCREENSHOT: /path/to/file.png` lines. Read the PNG files to inspect rendering.

### Run specific test

```bash
./build/run_tests --test BasicFormatting
```

Available tests:
- `BasicFormatting` - Headers, bold, italic, links, strikethrough
- `InlineBlock` - Inline code, fenced code blocks
- `BlockLayout` - Lists, blockquotes, tables, scroll behavior

### Workflow

1. Make code changes
2. Run `make test`
3. Read screenshot PNGs from output to verify changes
4. Iterate as needed

### Adding a quick visual check

For one-off verification, add a test case or modify an existing one in `tests/`:

```cpp
ctx.setContent("# Your markdown here\n\nTest content.");
screenshots.push_back(ctx.captureScreenshot("your_test_name"));
```

### Output location

Screenshots save to `/tmp/promark_tests/` by default. Override with:

```bash
TEST_OUTPUT_DIR=/custom/path ./build/run_tests
```

## Test Framework Details

| File | Purpose |
|------|---------|
| `tests/test_helpers.h` | TestContext, TestRunner, macros |
| `tests/test_helpers.cpp` | Offscreen FBO rendering, PNG export |
| `tests/test_*.cpp` | Individual test suites |
| `tests/run_tests.cpp` | Entry point |

The framework uses:
- Invisible GLFW window (`GLFW_VISIBLE=GLFW_FALSE`)
- OpenGL framebuffer object for offscreen rendering
- stb_image_write for PNG export
