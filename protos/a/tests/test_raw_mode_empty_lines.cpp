#include "test_helpers.h"
#include <iostream>

// Test clicking on empty lines in raw mode
TestResult test_raw_mode_click_empty_line(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Content with empty line between heading and paragraph
    std::string content = "## Heading\n\nParagraph text here.";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Switch to raw mode
    ctx.simulateKey(GLFW_KEY_R, GLFW_MOD_CONTROL);
    engine->render(ctx.getWidth(), ctx.getHeight());

    screenshots.push_back(ctx.captureScreenshot("raw_empty_line"));

    // In raw mode:
    // Line 1: "## Heading" (positions 0-10, newline at 10)
    // Line 2: "" (empty line, position 11 is the \n character)
    // Line 3: "Paragraph text here." (positions 12+)

    // Try to click on the empty line (line 2)
    // Looking at the actual screenshots:
    // - Line 1 "## Heading" at Y ~65-85
    // - Line 2 (empty) at Y ~85-105
    // - Line 3 "Paragraph..." at Y ~105-125

    // Click in the middle of line 2 (empty line)
    float clickX = 50;  // Near left margin
    float clickY = 95;  // Middle of the empty line area

    ctx.simulateClick(clickX, clickY);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int cursorPos = engine->getCursorPosition();
    std::cout << "  After click at (" << clickX << ", " << clickY << "): cursorPos=" << cursorPos << std::endl;

    screenshots.push_back(ctx.captureScreenshot("raw_empty_line", 1));

    // Expected position: 11 (the newline character of the empty line)
    // Or possibly 12 if we're at the start of "Paragraph"
    // The key is that we should NOT be on line 1 (positions 0-10) or somewhere random

    // Check that cursor is on or near the empty line (position 11 or 12)
    if (cursorPos < 11 || cursorPos > 12) {
        std::cerr << "  Expected cursor near position 11-12 (empty line) but got " << cursorPos << std::endl;
        // Show what character is at the position
        if (cursorPos >= 0 && cursorPos < static_cast<int>(content.length())) {
            char c = content[cursorPos];
            std::cerr << "  Character at cursor: '" << (c == '\n' ? "\\n" : std::string(1, c)) << "'" << std::endl;
        }
        return TestResult{"raw_mode_click_empty_line", false,
            "Click on empty line did not place cursor correctly", screenshots};
    }

    std::cout << "  Success: cursor placed at position " << cursorPos << " (on/near empty line)" << std::endl;
    TEST_PASS();
}

REGISTER_TEST(raw_mode_click_empty_line, test_raw_mode_click_empty_line);
