#include "test_helpers.h"
#include <iostream>

// Comprehensive test to verify "sticky caret" bug is fixed
// Tests various edge cases that could cause "stuck" behavior

// Test: Navigate through lines of varying lengths
// Expected: Cursor should move smoothly without getting stuck
TestResult test_varying_line_lengths(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create lines of varying lengths to test sticky column behavior
    std::string content = "ABCDEFGHIJ\nAB\nABCDEFGHIJ\nABC\nABCDE";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_verify", 0));

    // Click at position 5 of line 1 (between E and F)
    ctx.simulateClick(72, 80);  // Approximate position for column 5
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_verify", 1));

    // Navigate down through all lines, verifying cursor doesn't get stuck
    for (int i = 0; i < 5; i++) {
        ctx.simulateKey(GLFW_KEY_DOWN);
        engine->render(400, 300);
    }
    screenshots.push_back(ctx.captureScreenshot("sticky_verify", 2));

    // Type marker to verify we're on last line
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);
    std::string result = engine->getContent();

    // Count newlines to verify we reached the last line
    int nlCount = 0;
    size_t zPos = result.find('z');
    for (size_t i = 0; i < zPos && i < result.length(); i++) {
        if (result[i] == '\n') nlCount++;
    }

    std::cout << "  Content: '" << result << "'" << std::endl;
    std::cout << "  'z' at pos " << zPos << ", after " << nlCount << " newlines" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_verify", 3));

    // Should be on last line (4 newlines before 'z')
    if (nlCount < 3) {
        return TestResult{"test_varying_line_lengths", false,
            "Cursor got stuck - didn't reach last line after repeated downs. Only " +
            std::to_string(nlCount) + " newlines before 'z'", screenshots};
    }

    TEST_PASS();
}

// Test: Navigate up through lines of varying lengths
TestResult test_up_varying_lengths(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "ABCDE\nABC\nABCDEFGHIJ\nAB\nABCDEFGHIJ";
    engine->setContent(content);
    engine->render(400, 300);

    // Click on last line
    ctx.simulateClick(72, 150);  // Approximate y position for line 5
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_up_verify", 0));

    // Navigate up through all lines
    for (int i = 0; i < 5; i++) {
        ctx.simulateKey(GLFW_KEY_UP);
        engine->render(400, 300);
    }
    screenshots.push_back(ctx.captureScreenshot("sticky_up_verify", 1));

    // Type marker to verify we're on first line
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);
    std::string result = engine->getContent();

    size_t zPos = result.find('z');
    size_t firstNl = result.find('\n');

    std::cout << "  Content: '" << result << "'" << std::endl;
    std::cout << "  'z' at pos " << zPos << ", first newline at " << firstNl << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_up_verify", 2));

    // Should be on first line (z before first newline)
    if (zPos > firstNl) {
        return TestResult{"test_up_varying_lengths", false,
            "Cursor got stuck - didn't reach first line after repeated ups", screenshots};
    }

    TEST_PASS();
}

// Test: Mixed navigation (up, down, left, right)
TestResult test_mixed_navigation(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "12345\nABCDE\n67890";
    engine->setContent(content);
    engine->render(400, 300);

    // Start at beginning
    ctx.simulateClick(32, 80);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_mixed", 0));

    // Right 2, Down 1, Left 1, Up 1 - should end up at position 1 on line 1
    ctx.simulateKey(GLFW_KEY_RIGHT);
    ctx.simulateKey(GLFW_KEY_RIGHT);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_mixed", 1));

    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_mixed", 2));

    ctx.simulateKey(GLFW_KEY_LEFT);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_mixed", 3));

    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_mixed", 4));

    // Type marker
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);
    std::string result = engine->getContent();

    std::cout << "  Content after mixed nav: '" << result << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_mixed", 5));

    // Verify 'z' is on line 1 (before first newline)
    size_t zPos = result.find('z');
    size_t firstNl = result.find('\n');

    if (zPos == std::string::npos || zPos > firstNl) {
        return TestResult{"test_mixed_navigation", false,
            "Mixed navigation failed - cursor not on line 1. z at " +
            std::to_string(zPos) + ", nl at " + std::to_string(firstNl), screenshots};
    }

    TEST_PASS();
}

// Test: Empty line navigation (common source of sticky behavior)
TestResult test_empty_line_navigation(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Multiple empty lines - common edge case
    std::string content = "LINE1\n\n\nLINE4";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_empty", 0));

    // Click on line 1
    ctx.simulateClick(48, 80);
    engine->render(400, 300);

    // Navigate down through empty lines to line 4
    for (int i = 0; i < 4; i++) {
        ctx.simulateKey(GLFW_KEY_DOWN);
        engine->render(400, 300);
    }
    screenshots.push_back(ctx.captureScreenshot("sticky_empty", 1));

    // Type marker
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);
    std::string result = engine->getContent();

    std::cout << "  Content with empty lines: '" << result << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_empty", 2));

    // Count newlines before 'z' - should be 3 (to be on LINE4)
    size_t zPos = result.find('z');
    int nlCount = 0;
    for (size_t i = 0; i < zPos && i < result.length(); i++) {
        if (result[i] == '\n') nlCount++;
    }

    std::cout << "  'z' after " << nlCount << " newlines" << std::endl;

    if (nlCount < 2) {
        return TestResult{"test_empty_line_navigation", false,
            "Cursor stuck on empty lines - only " + std::to_string(nlCount) +
            " newlines before 'z'", screenshots};
    }

    TEST_PASS();
}

REGISTER_TEST(varying_line_lengths, test_varying_line_lengths);
REGISTER_TEST(up_varying_lengths, test_up_varying_lengths);
REGISTER_TEST(mixed_navigation, test_mixed_navigation);
REGISTER_TEST(empty_line_navigation, test_empty_line_navigation);
