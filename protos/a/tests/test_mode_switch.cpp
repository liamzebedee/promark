#include "test_helpers.h"
#include <iostream>

// Test that caret position is preserved when switching between visual and raw modes
TestResult test_mode_switch_heading(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set up content with heading (has hidden "## " prefix in visual mode)
    std::string content = "## Peace and work.\n\nOne of the ways to approach life.";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Position cursor at "P" in "Peace" (raw position 3, after "## ")
    int initialRawPos = 3;
    engine->setCursorPosition(initialRawPos);
    engine->render(ctx.getWidth(), ctx.getHeight());

    std::cout << "  Initial (visual mode): rawPos=" << engine->getCursorPosition() << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_heading"));

    // Switch to raw mode (press 'r' or use API if available)
    // For now, simulate clicking the Raw button or use keyboard shortcut
    // The Engine toggles showRaw when 'r' is pressed (need to check)
    ctx.simulateKey(GLFW_KEY_R, GLFW_MOD_CONTROL);  // Toggle raw mode
    engine->render(ctx.getWidth(), ctx.getHeight());

    int rawModePos = engine->getCursorPosition();
    std::cout << "  After switch to raw: rawPos=" << rawModePos << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_heading", 1));

    // Switch back to visual mode
    ctx.simulateKey(GLFW_KEY_R, GLFW_MOD_CONTROL);  // Toggle back
    engine->render(ctx.getWidth(), ctx.getHeight());

    int finalPos = engine->getCursorPosition();
    std::cout << "  After switch back: rawPos=" << finalPos << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_heading", 2));

    // Round trip should preserve position
    if (finalPos != initialRawPos) {
        std::cerr << "  FAIL: Position changed from " << initialRawPos
                  << " to " << finalPos << " after round trip" << std::endl;
        return TestResult{"mode_switch_heading", false,
            "Position changed after mode switch round trip", screenshots};
    }

    TEST_PASS();
}

// Test mode switch with caret inside bold text
TestResult test_mode_switch_bold(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Content with bold text
    std::string content = "Hello **world** there.";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Position cursor at 'o' in "world"
    // Raw: "Hello **world** there."
    //       0123456789...
    // 'w' is at raw position 8
    // 'o' is at raw position 9
    int initialRawPos = 9;
    engine->setCursorPosition(initialRawPos);
    engine->render(ctx.getWidth(), ctx.getHeight());

    std::cout << "  Initial (visual): rawPos=" << engine->getCursorPosition() << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_bold"));

    // Switch to raw mode
    ctx.simulateKey(GLFW_KEY_R, GLFW_MOD_CONTROL);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int rawModePos = engine->getCursorPosition();
    std::cout << "  After raw: rawPos=" << rawModePos << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_bold", 1));

    // Switch back
    ctx.simulateKey(GLFW_KEY_R, GLFW_MOD_CONTROL);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int finalPos = engine->getCursorPosition();
    std::cout << "  After visual: rawPos=" << finalPos << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_bold", 2));

    if (finalPos != initialRawPos) {
        std::cerr << "  FAIL: Position changed from " << initialRawPos
                  << " to " << finalPos << std::endl;
        return TestResult{"mode_switch_bold", false,
            "Position changed after mode switch", screenshots};
    }

    TEST_PASS();
}

// Test mode switch with caret inside a link
TestResult test_mode_switch_link(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Content with link
    std::string content = "Check out [Google](https://google.com) please.";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Position cursor at 'o' in "Google"
    // Raw: "Check out [Google](https://google.com) please."
    //       0123456789012345...
    // 'G' is at raw position 11
    // 'o' is at raw position 12
    int initialRawPos = 12;
    engine->setCursorPosition(initialRawPos);
    engine->render(ctx.getWidth(), ctx.getHeight());

    std::cout << "  Initial: rawPos=" << engine->getCursorPosition() << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_link"));

    // Switch to raw mode
    ctx.simulateKey(GLFW_KEY_R, GLFW_MOD_CONTROL);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int rawModePos = engine->getCursorPosition();
    std::cout << "  After raw: rawPos=" << rawModePos << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_link", 1));

    // Switch back
    ctx.simulateKey(GLFW_KEY_R, GLFW_MOD_CONTROL);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int finalPos = engine->getCursorPosition();
    std::cout << "  After visual: rawPos=" << finalPos << std::endl;
    screenshots.push_back(ctx.captureScreenshot("mode_switch_link", 2));

    if (finalPos != initialRawPos) {
        std::cerr << "  FAIL: Position changed from " << initialRawPos
                  << " to " << finalPos << std::endl;
        return TestResult{"mode_switch_link", false,
            "Position changed after mode switch", screenshots};
    }

    TEST_PASS();
}

REGISTER_TEST(mode_switch_heading, test_mode_switch_heading);
REGISTER_TEST(mode_switch_bold, test_mode_switch_bold);
REGISTER_TEST(mode_switch_link, test_mode_switch_link);
