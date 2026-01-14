#include "test_helpers.h"
#include "../src/engine/layout_objects.h"
#include "../src/engine/markdown_parser.h"
#include "../src/engine/layout_engine.h"
#include "../src/engine/text_buffer.h"
#include "../src/engine/markdown_renderer.h"
#include "../src/engine/typography.h"
#include <iostream>
#include <cmath>

// Debug test to understand the vertical navigation issue
// This test manually traces through the logic to find the bug

TestResult test_debug_vertical_nav(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "LINE1\nLINE2\nLINE3";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("debug_vnav", 0));

    std::cout << "\n=== DEBUG VERTICAL NAVIGATION ===" << std::endl;
    std::cout << "Content: '" << content << "'" << std::endl;
    std::cout << "Content length: " << content.length() << std::endl;

    // Click to position cursor at start of line 1
    // Line 1 is at y=35.2-51.2 in content space, +40 for toolbar = 75.2-91.2
    ctx.simulateClick(35, 80);  // Use x=35 (within line), y=80 (line 1)
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("debug_vnav", 1));

    // Type marker to see where we are
    ctx.simulateKey(GLFW_KEY_A);
    engine->render(400, 300);
    std::string after1 = engine->getContent();
    std::cout << "After click + 'a': '" << after1 << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("debug_vnav", 2));

    // Reset and try Down navigation
    engine->setContent(content);
    engine->render(400, 300);
    ctx.simulateClick(32, 80);
    engine->render(400, 300);

    // Try multiple Downs
    std::cout << "\nTrying Down navigation:" << std::endl;
    for (int i = 0; i < 3; i++) {
        ctx.simulateKey(GLFW_KEY_DOWN);
        engine->render(400, 300);

        // Insert a digit to mark position
        ctx.simulateKey(GLFW_KEY_0 + i + 1);  // Insert '1', '2', '3'
        engine->render(400, 300);

        std::string afterDown = engine->getContent();
        std::cout << "  After Down " << (i+1) << " + digit: '" << afterDown << "'" << std::endl;

        // Reset for next iteration
        engine->setContent(content);
        engine->render(400, 300);
        ctx.simulateClick(32, 80);
        engine->render(400, 300);

        // Apply the same number of downs
        for (int j = 0; j <= i; j++) {
            ctx.simulateKey(GLFW_KEY_DOWN);
            engine->render(400, 300);
        }
    }

    screenshots.push_back(ctx.captureScreenshot("debug_vnav", 3));

    // Check what position Down actually takes us to
    engine->setContent(content);
    engine->render(400, 300);

    // Click on line 1
    ctx.simulateClick(48, 80);
    engine->render(400, 300);

    // Insert marker before Down
    ctx.simulateKey(GLFW_KEY_X);
    engine->render(400, 300);
    std::string beforeDown = engine->getContent();
    std::cout << "\nBefore Down (clicked L1, typed X): '" << beforeDown << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("debug_vnav", 4));

    // Now press Down
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("debug_vnav", 5));

    // Insert marker after Down
    ctx.simulateKey(GLFW_KEY_Y);
    engine->render(400, 300);
    std::string afterDown = engine->getContent();
    std::cout << "After Down (typed Y): '" << afterDown << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("debug_vnav", 6));

    // Find positions
    size_t xPos = afterDown.find('x');
    size_t yPos = afterDown.find('y');
    size_t nl1 = afterDown.find('\n');
    size_t nl2 = afterDown.find('\n', nl1 + 1);

    std::cout << "'X' at pos " << xPos << ", 'Y' at pos " << yPos << std::endl;
    std::cout << "First newline at " << nl1 << ", second at " << nl2 << std::endl;

    // Y should be between nl1 and nl2 (on line 2)
    bool yOnLine2 = (yPos > nl1 && yPos < nl2);
    std::cout << "Y on line 2? " << (yOnLine2 ? "YES" : "NO") << std::endl;

    if (!yOnLine2) {
        // Which line is Y actually on?
        if (yPos <= nl1) {
            std::cout << "Y is still on LINE 1 - Down didn't move!" << std::endl;
        } else if (yPos > nl2) {
            std::cout << "Y jumped to LINE 3 - Down went too far!" << std::endl;
        }

        return TestResult{"test_debug_vertical_nav", false,
            "Down key doesn't navigate correctly. Y at " + std::to_string(yPos) +
            " should be between " + std::to_string(nl1) + " and " + std::to_string(nl2),
            screenshots};
    }

    TEST_PASS();
}

REGISTER_TEST(debug_vertical_nav, test_debug_vertical_nav);
