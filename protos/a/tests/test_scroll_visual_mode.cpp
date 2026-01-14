#include "test_helpers.h"
#include "../src/engine/typography.h"
#include <iostream>

// Test that scrolling in visual mode shows content
// Bug: Scrolling down shows nothing due to incorrect viewport culling
TestResult test_scroll_visual_mode(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create content that's longer than the viewport (600px)
    // Each paragraph adds roughly 50-80px of height
    std::string content =
        "# Heading at Top\n\n"
        "Paragraph one - this is the first paragraph that should be visible at scroll position 0.\n\n"
        "Paragraph two - more content to fill the page.\n\n"
        "Paragraph three - even more content.\n\n"
        "Paragraph four - getting into content that may be below the fold.\n\n"
        "Paragraph five - definitely below initial viewport.\n\n"
        "# Heading in Middle\n\n"
        "Paragraph six - this content should appear when scrolling down.\n\n"
        "Paragraph seven - more scrollable content.\n\n"
        "Paragraph eight - continuing to scroll.\n\n"
        "# Heading Near End\n\n"
        "Paragraph nine - near the bottom of the document.\n\n"
        "Paragraph ten - this is the final paragraph at the very end of the document.";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Screenshot 1: Initial view (scroll=0) - should show heading and first paragraphs
    screenshots.push_back(ctx.captureScreenshot("scroll_visual_mode", 0));
    std::cout << "  Screenshot 0: Initial view at scroll=0" << std::endl;

    // Scroll down by 200 pixels
    ctx.simulateScroll(-5);  // Negative = scroll down (mouse wheel direction)
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_visual_mode", 1));
    std::cout << "  Screenshot 1: After scrolling down ~200px" << std::endl;

    // Scroll down more
    ctx.simulateScroll(-5);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_visual_mode", 2));
    std::cout << "  Screenshot 2: After scrolling down more (~400px total)" << std::endl;

    // Scroll down even more to see content at the end
    ctx.simulateScroll(-10);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_visual_mode", 3));
    std::cout << "  Screenshot 3: Near the end of document" << std::endl;

    // BUG: Screenshots 1-3 should show content, but currently show nothing
    // because viewport culling uses wrong coordinate system

    TEST_PASS();
}

// Test that content remains visible after scrolling back up
TestResult test_scroll_up_down(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content =
        "# Top of Document\n\n"
        "This is the first paragraph.\n\n"
        "This is the second paragraph.\n\n"
        "This is the third paragraph.\n\n"
        "This is the fourth paragraph.\n\n"
        "This is the fifth paragraph.\n\n"
        "# Middle Section\n\n"
        "This is content in the middle.\n\n"
        "More middle content.\n\n"
        "# Bottom Section\n\n"
        "This is near the bottom.\n\n"
        "Final paragraph at the end.";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Initial view
    screenshots.push_back(ctx.captureScreenshot("scroll_up_down", 0));
    std::cout << "  Screenshot 0: Initial view" << std::endl;

    // Scroll down
    ctx.simulateScroll(-10);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_up_down", 1));
    std::cout << "  Screenshot 1: After scrolling down" << std::endl;

    // Scroll back up
    ctx.simulateScroll(10);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scroll_up_down", 2));
    std::cout << "  Screenshot 2: After scrolling back up" << std::endl;

    TEST_PASS();
}

// Test clicking after scrolling (hit testing must account for scroll offset)
TestResult test_click_after_scroll(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content =
        "# Top Heading\n\n"
        "First paragraph at the top.\n\n"
        "Second paragraph.\n\n"
        "Third paragraph.\n\n"
        "Fourth paragraph.\n\n"
        "Fifth paragraph.\n\n"
        "# Middle Heading\n\n"
        "Sixth paragraph - click target after scroll.\n\n"
        "Seventh paragraph.\n\n"
        "Eighth paragraph.\n\n"
        "# Bottom Heading\n\n"
        "Final content at the bottom.";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_after_scroll", 0));
    std::cout << "  Screenshot 0: Initial view" << std::endl;

    // Scroll down to show middle content
    ctx.simulateScroll(-10);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_after_scroll", 1));
    std::cout << "  Screenshot 1: After scrolling down" << std::endl;

    // Click on content that's now visible (previously below fold)
    // Should position cursor correctly at clicked location
    float clickY = 150;  // Should hit content in the middle section
    ctx.simulateClick(100, clickY);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_after_scroll", 2));
    std::cout << "  Screenshot 2: After clicking at y=" << clickY << " while scrolled" << std::endl;

    // Type 'X' to verify cursor is in correct position
    ctx.simulateKey(GLFW_KEY_X);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("click_after_scroll", 3));
    std::cout << "  Screenshot 3: After typing 'X' - cursor should be at clicked position" << std::endl;

    TEST_PASS();
}

// Test auto-scroll keeps cursor visible when typing at bottom
TestResult test_auto_scroll_cursor(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Start with content that fits in viewport
    std::string content = "Start typing here...";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Move cursor to end
    ctx.simulateKey(GLFW_KEY_END, GLFW_MOD_CONTROL);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("auto_scroll_cursor", 0));
    std::cout << "  Screenshot 0: Cursor at end" << std::endl;

    // Add many lines by typing Enter repeatedly
    for (int i = 0; i < 20; i++) {
        ctx.simulateKey(GLFW_KEY_ENTER);
        ctx.simulateKey(GLFW_KEY_L);
        ctx.simulateKey(GLFW_KEY_I);
        ctx.simulateKey(GLFW_KEY_N);
        ctx.simulateKey(GLFW_KEY_E);
    }
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("auto_scroll_cursor", 1));
    std::cout << "  Screenshot 1: After adding 20 lines - cursor should remain visible" << std::endl;

    TEST_PASS();
}

REGISTER_TEST(scroll_visual_mode, test_scroll_visual_mode);
REGISTER_TEST(scroll_up_down, test_scroll_up_down);
REGISTER_TEST(click_after_scroll, test_click_after_scroll);
REGISTER_TEST(auto_scroll_cursor, test_auto_scroll_cursor);
