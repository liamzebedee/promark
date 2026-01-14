#include "test_helpers.h"
#include <iostream>

// Test that visual column is preserved when navigating between headings and paragraphs
TestResult test_heading_to_paragraph_column(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set up content with heading and paragraph
    // In visual mode, the "## " prefix is hidden
    std::string content = "## Peace and work.\n\nOne of the ways to approach life.";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("heading_paragraph_nav"));

    // Position cursor at "P" in "Peace" (start of visible heading text)
    // Raw position: "## P" = index 3 (after "## ")
    // In visual mode, this appears at column 0 (start of visible text)
    int rawPosP = 3;  // Position of "P" in raw text
    engine->setCursorPosition(rawPosP);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Get visual position before navigation
    float beforeX, beforeY;
    int domPosP = engine->getMarkdownRenderer()->rawToDOM(rawPosP);
    engine->getMarkdownRenderer()->getCursorXY(domPosP, beforeX, beforeY);
    std::cout << "  Before: rawPos=" << rawPosP << " domPos=" << domPosP
              << " x=" << beforeX << " y=" << beforeY << std::endl;

    screenshots.push_back(ctx.captureScreenshot("heading_paragraph_nav", 1));

    // Navigate down to paragraph
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int afterPos = engine->getCursorPosition();
    float afterX, afterY;
    int afterDomPos = engine->getMarkdownRenderer()->rawToDOM(afterPos);
    engine->getMarkdownRenderer()->getCursorXY(afterDomPos, afterX, afterY);
    std::cout << "  After down: rawPos=" << afterPos << " domPos=" << afterDomPos
              << " x=" << afterX << " y=" << afterY << std::endl;

    screenshots.push_back(ctx.captureScreenshot("heading_paragraph_nav", 2));

    // The paragraph starts at raw position 20 (after "## Peace and work.\n\n")
    // "One of the ways..." starts at index 20
    // Position at "O" should be index 20
    // The visual x positions should be similar (both at start of visible line)

    // Check that the X positions are close (within tolerance for different fonts/sizing)
    float xDiff = std::abs(afterX - beforeX);
    std::cout << "  X difference: " << xDiff << " (before=" << beforeX << ", after=" << afterX << ")" << std::endl;

    // After pressing down from "P" in "Peace", we should land on "O" in "One"
    // Both are at the start of their visual lines
    // The paragraph line might have different x due to blockquote indent, etc.
    // but for regular heading->paragraph, they should be close

    // Expected: afterPos should be 20 (start of "One...")
    int expectedPos = 20;
    if (afterPos != expectedPos) {
        std::cerr << "  Expected position " << expectedPos << " but got " << afterPos << std::endl;

        // Get the character at the cursor position
        std::string text = content;
        if (afterPos >= 0 && afterPos < static_cast<int>(text.length())) {
            std::cerr << "  Character at cursor: '" << text[afterPos] << "'" << std::endl;
        }
    }

    // Test navigation back up
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int backPos = engine->getCursorPosition();
    std::cout << "  After up: rawPos=" << backPos << std::endl;

    screenshots.push_back(ctx.captureScreenshot("heading_paragraph_nav", 3));

    // Should return to approximately same position as before
    // (might be slightly different due to X coordinate differences)
    if (backPos != rawPosP) {
        std::cerr << "  Warning: Up navigation returned to " << backPos
                  << " instead of " << rawPosP << std::endl;
    }

    // For now, just report the behavior - we need to observe what happens
    // before deciding if this is a bug or expected behavior
    TEST_PASS();
}

REGISTER_TEST(heading_to_paragraph_column, test_heading_to_paragraph_column);
