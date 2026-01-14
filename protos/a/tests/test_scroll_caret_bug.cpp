#include "test_helpers.h"
#include <iostream>

// Test caret positioning after scrolling - this might reveal bugs
// that don't appear in simple non-scrolling tests

TestResult test_caret_after_scroll_and_typing(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create content that requires scrolling (many paragraphs)
    std::string content = R"(# First Section

This is the first paragraph of content.

This is the second paragraph of content.

This is the third paragraph of content.

# Second Section

This is paragraph four.

This is paragraph five.

This is paragraph six.

# Third Section

This is paragraph seven.

This is paragraph eight with more text to make it longer and wrap around.

This is paragraph nine - we will click here after scrolling.)";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_caret_bug", 0));

    // Scroll down to see paragraph nine
    ctx.simulateScroll(-10);  // Negative = scroll down
    for (int i = 0; i < 5; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_caret_bug", 1));

    // Click in paragraph nine (approximately)
    ctx.simulateClick(150, 300);
    for (int i = 0; i < 5; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_caret_bug", 2));

    // Type some text
    const char* text = "inserted";
    for (const char* p = text; *p; p++) {
        int key = GLFW_KEY_A + (*p - 'a');
        ctx.simulateKey(key);
    }
    for (int i = 0; i < 5; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_caret_bug", 3));

    // Press Enter
    ctx.simulateKey(GLFW_KEY_ENTER);
    for (int i = 0; i < 10; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_caret_bug", 4));

    // Type more text
    ctx.simulateKey(GLFW_KEY_N);
    ctx.simulateKey(GLFW_KEY_E);
    ctx.simulateKey(GLFW_KEY_W);
    for (int i = 0; i < 10; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_caret_bug", 5));

    std::cout << "  Final content excerpt:" << std::endl;
    std::string finalContent = engine->getContent();
    size_t pos = finalContent.find("inserted");
    if (pos != std::string::npos) {
        size_t start = (pos > 50) ? pos - 50 : 0;
        size_t end = std::min(pos + 100, finalContent.length());
        std::cout << "  ..." << finalContent.substr(start, end - start) << "..." << std::endl;
    }

    TEST_PASS();
}

REGISTER_TEST(caret_after_scroll_and_typing, test_caret_after_scroll_and_typing);
