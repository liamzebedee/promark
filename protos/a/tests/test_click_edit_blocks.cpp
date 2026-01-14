#include "test_helpers.h"
#include <iostream>

// Test clicking and editing inside various block elements
// Bug: Need to verify click-to-edit works correctly inside block elements

TestResult test_click_edit_code_block(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = R"(# Test Document

Some intro text here.

```python
def hello():
    print("Hello")
```

Text after code block.
)";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_code_block", 0));
    std::cout << "  Step 1: Loaded document with code block" << std::endl;

    // Try to click inside the code block (approximately where "print" is)
    // Code block should be around y=200-250 depending on layout
    ctx.simulateClick(100, 220);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_code_block", 1));
    std::cout << "  Step 2: Clicked inside code block" << std::endl;

    // Type some text to verify we can edit inside the code block
    ctx.simulateKey(GLFW_KEY_X);
    ctx.simulateKey(GLFW_KEY_Y);
    ctx.simulateKey(GLFW_KEY_Z);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_code_block", 2));
    std::cout << "  Step 3: Typed 'xyz' inside code block" << std::endl;

    std::string finalContent = engine->getContent();
    std::cout << "  Final content preview: " << finalContent.substr(0, 200) << "..." << std::endl;

    TEST_PASS();
}

TestResult test_click_edit_blockquote(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = R"(# Block Quote Test

> This is a quote
> spanning two lines

Text after quote.
)";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_blockquote", 0));
    std::cout << "  Step 1: Loaded document with block quote" << std::endl;

    // Click inside the blockquote - need to target where "This is a quote" text is
    // Block quote starts around y=165 based on visual inspection
    ctx.simulateClick(120, 170);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_blockquote", 1));
    std::cout << "  Step 2: Clicked inside block quote" << std::endl;

    // Type some text
    ctx.simulateKey(GLFW_KEY_T);
    ctx.simulateKey(GLFW_KEY_E);
    ctx.simulateKey(GLFW_KEY_S);
    ctx.simulateKey(GLFW_KEY_T);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_blockquote", 2));
    std::cout << "  Step 3: Typed 'test' inside block quote" << std::endl;

    std::string finalContent = engine->getContent();
    std::cout << "  Final content: " << finalContent << std::endl;

    TEST_PASS();
}

TestResult test_click_edit_nested_list(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = R"(# List Test

- First item
  - Nested item one
  - Nested item two
- Second item
)";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_nested_list", 0));
    std::cout << "  Step 1: Loaded document with nested list" << std::endl;

    // Click inside the nested list item
    ctx.simulateClick(200, 160);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_nested_list", 1));
    std::cout << "  Step 2: Clicked inside nested list item" << std::endl;

    // Type some text
    ctx.simulateKey(GLFW_KEY_A);
    ctx.simulateKey(GLFW_KEY_B);
    ctx.simulateKey(GLFW_KEY_C);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_nested_list", 2));
    std::cout << "  Step 3: Typed 'abc' in nested list" << std::endl;

    std::string finalContent = engine->getContent();
    std::cout << "  Final content: " << finalContent << std::endl;

    TEST_PASS();
}

TestResult test_click_edit_header(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = R"(# Main Header

Some paragraph text.

## Second Header

More text here.
)";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_header", 0));
    std::cout << "  Step 1: Loaded document with headers" << std::endl;

    // Click inside the H2 header
    ctx.simulateClick(150, 180);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_header", 1));
    std::cout << "  Step 2: Clicked inside H2 header" << std::endl;

    // Type some text
    ctx.simulateKey(GLFW_KEY_N);
    ctx.simulateKey(GLFW_KEY_E);
    ctx.simulateKey(GLFW_KEY_W);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_header", 2));
    std::cout << "  Step 3: Typed 'new' in header" << std::endl;

    std::string finalContent = engine->getContent();
    std::cout << "  Final content: " << finalContent << std::endl;

    TEST_PASS();
}

REGISTER_TEST(click_edit_code_block, test_click_edit_code_block);
REGISTER_TEST(click_edit_blockquote, test_click_edit_blockquote);
REGISTER_TEST(click_edit_nested_list, test_click_edit_nested_list);
REGISTER_TEST(click_edit_header, test_click_edit_header);
