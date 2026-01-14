#include "test_helpers.h"
#include "../src/engine/layout_objects.h"
#include "../src/engine/markdown_parser.h"
#include "../src/engine/layout_engine.h"
#include "../src/engine/text_buffer.h"
#include <iostream>
#include <cmath>

// Test that explores "sticky" caret behavior
// Bug report: Caret gets "stuck" in weird positions during arrow key navigation

// Test repeated left/right navigation by verifying typed character position
TestResult test_left_right_navigation(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "Hello world test";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_lr", 0));

    // Click to position cursor after "Hello " (approximately)
    ctx.simulateClick(80, 86);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_lr", 1));

    // Move right 3 times
    for (int i = 0; i < 3; i++) {
        ctx.simulateKey(GLFW_KEY_RIGHT);
        engine->render(400, 300);
    }
    screenshots.push_back(ctx.captureScreenshot("sticky_lr", 2));

    // Move left 3 times - should return to original position
    for (int i = 0; i < 3; i++) {
        ctx.simulateKey(GLFW_KEY_LEFT);
        engine->render(400, 300);
    }
    screenshots.push_back(ctx.captureScreenshot("sticky_lr", 3));

    // Type a marker character to verify position
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);

    std::string result = engine->getContent();
    std::cout << "  Content after left/right test: '" << result << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_lr", 4));

    // Verify the 'z' was inserted - should be somewhere in the middle
    size_t zPos = result.find('z');
    if (zPos == std::string::npos) {
        return TestResult{"test_left_right_navigation", false,
            "Marker 'z' not found in content", screenshots};
    }

    std::cout << "  'z' inserted at position: " << zPos << std::endl;

    TEST_PASS();
}

// Test up/down navigation between lines with different lengths
TestResult test_up_down_at_boundaries(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create 3 lines of different lengths
    std::string content = "Short\nThis is a much longer line\nTiny";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_ud_boundary", 0));

    // Click at end of first line
    ctx.simulateClick(60, 86);  // Near end of "Short"
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_ud_boundary", 1));

    // Move down to second line
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_ud_boundary", 2));

    // Move down to third line
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_ud_boundary", 3));

    // Move up twice - should return to first line
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_ud_boundary", 4));

    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_ud_boundary", 5));

    // Type a marker to verify we're on first line
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);

    std::string result = engine->getContent();
    std::cout << "  Content after up/down test: '" << result << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_ud_boundary", 6));

    // The 'z' should be on the first line (before the first \n)
    size_t firstNewline = result.find('\n');
    size_t zPos = result.find('z');

    if (zPos == std::string::npos) {
        return TestResult{"test_up_down_at_boundaries", false,
            "Marker 'z' not found in content", screenshots};
    }

    if (zPos > firstNewline) {
        return TestResult{"test_up_down_at_boundaries", false,
            "Marker 'z' at position " + std::to_string(zPos) +
            " is after first newline at " + std::to_string(firstNewline) +
            " - cursor didn't return to first line",
            screenshots};
    }

    std::cout << "  'z' correctly on first line at position " << zPos << std::endl;

    TEST_PASS();
}

// Test that pressing Up multiple times at first line doesn't break things
TestResult test_up_at_first_line(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "First line here\nSecond line";
    engine->setContent(content);
    engine->render(400, 300);

    // Click middle of first line
    ctx.simulateClick(80, 86);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_first_line", 0));

    // Press Up 5 times - should stay on first line, not get stuck
    for (int i = 0; i < 5; i++) {
        ctx.simulateKey(GLFW_KEY_UP);
        engine->render(400, 300);
    }
    screenshots.push_back(ctx.captureScreenshot("sticky_first_line", 1));

    // Type a marker
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);

    std::string result = engine->getContent();
    std::cout << "  Content after up-at-first test: '" << result << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_first_line", 2));

    // Verify 'z' is on first line
    size_t firstNewline = result.find('\n');
    size_t zPos = result.find('z');

    if (zPos == std::string::npos || zPos > firstNewline) {
        return TestResult{"test_up_at_first_line", false,
            "Pressing Up at first line moved cursor off first line", screenshots};
    }

    TEST_PASS();
}

// Test that pressing Down multiple times at last line doesn't break things
TestResult test_down_at_last_line(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "First line\nLast line here";
    engine->setContent(content);
    engine->render(400, 300);

    // Click middle of last line
    ctx.simulateClick(80, 102);  // Second line y position
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_last_line", 0));

    // Press Down 5 times - should stay on last line, not get stuck
    for (int i = 0; i < 5; i++) {
        ctx.simulateKey(GLFW_KEY_DOWN);
        engine->render(400, 300);
    }
    screenshots.push_back(ctx.captureScreenshot("sticky_last_line", 1));

    // Type a marker
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);

    std::string result = engine->getContent();
    std::cout << "  Content after down-at-last test: '" << result << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_last_line", 2));

    // Verify 'z' is on last line (after the first newline)
    size_t firstNewline = result.find('\n');
    size_t zPos = result.find('z');

    if (zPos == std::string::npos || zPos < firstNewline) {
        return TestResult{"test_down_at_last_line", false,
            "Pressing Down at last line moved cursor to first line", screenshots};
    }

    TEST_PASS();
}

// Test navigation after typing - cursor should track properly
TestResult test_navigation_after_typing(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "Line one\nLine two";
    engine->setContent(content);
    engine->render(400, 300);

    // Click at end of first line
    ctx.simulateClick(120, 86);  // After "Line one"
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_after_type", 0));

    // Type some characters
    ctx.simulateKey(GLFW_KEY_X);
    ctx.simulateKey(GLFW_KEY_Y);
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_after_type", 1));

    // Now navigate left twice
    ctx.simulateKey(GLFW_KEY_LEFT);
    ctx.simulateKey(GLFW_KEY_LEFT);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_after_type", 2));

    // Navigate down to second line
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_after_type", 3));

    // Navigate back up
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_after_type", 4));

    // Type a marker - should be on first line
    ctx.simulateKey(GLFW_KEY_Q);
    engine->render(400, 300);

    std::string result = engine->getContent();
    std::cout << "  Content: '" << result << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_after_type", 5));

    // Verify 'q' is on first line
    size_t firstNewline = result.find('\n');
    size_t qPos = result.find('q');

    if (qPos == std::string::npos || qPos > firstNewline) {
        return TestResult{"test_navigation_after_typing", false,
            "Navigation after typing is broken - 'q' not on first line. Content: '" + result + "'",
            screenshots};
    }

    TEST_PASS();
}

// Test rapid mixed navigation
TestResult test_rapid_navigation(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "ABCDEFGHIJ\nKLMNOPQRST\nUVWXYZ1234";
    engine->setContent(content);
    engine->render(400, 300);

    // Start at middle of first line
    ctx.simulateClick(80, 86);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_rapid", 0));

    // Rapid navigation: Right-Down-Left-Up
    ctx.simulateKey(GLFW_KEY_RIGHT);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_LEFT);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_rapid", 1));

    // Type marker
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);

    std::string result = engine->getContent();
    std::cout << "  Content: '" << result << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_rapid", 2));

    // Verify 'z' is on first line
    size_t firstNewline = result.find('\n');
    size_t zPos = result.find('z');

    if (zPos == std::string::npos) {
        return TestResult{"test_rapid_navigation", false,
            "Marker 'z' not found", screenshots};
    }

    if (zPos > firstNewline) {
        return TestResult{"test_rapid_navigation", false,
            "Rapid navigation ended on wrong line (z at pos " + std::to_string(zPos) +
            ", first newline at " + std::to_string(firstNewline) + ")",
            screenshots};
    }

    TEST_PASS();
}

// Test: Home and End keys for line navigation
TestResult test_home_end_navigation(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "Hello world";
    engine->setContent(content);
    engine->render(400, 300);

    // Click in middle
    ctx.simulateClick(80, 86);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_home_end", 0));

    // Press Home - should go to start
    ctx.simulateKey(GLFW_KEY_HOME);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_home_end", 1));

    // Type marker at start
    ctx.simulateKey(GLFW_KEY_Z);
    engine->render(400, 300);

    std::string afterHome = engine->getContent();
    std::cout << "  After Home+z: '" << afterHome << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_home_end", 2));

    // 'z' should be at position 0 (start)
    if (afterHome[0] != 'z') {
        return TestResult{"test_home_end_navigation", false,
            "Home key didn't go to start. Content: '" + afterHome + "'", screenshots};
    }

    // Now test End - reset content
    engine->setContent("Hello world");
    engine->render(400, 300);

    // Click in middle
    ctx.simulateClick(80, 86);
    engine->render(400, 300);

    // Press End - should go to end
    ctx.simulateKey(GLFW_KEY_END);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("sticky_home_end", 3));

    // Type marker at end
    ctx.simulateKey(GLFW_KEY_Q);
    engine->render(400, 300);

    std::string afterEnd = engine->getContent();
    std::cout << "  After End+q: '" << afterEnd << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("sticky_home_end", 4));

    // 'q' should be at end
    if (afterEnd.back() != 'q') {
        return TestResult{"test_home_end_navigation", false,
            "End key didn't go to end. Content: '" + afterEnd + "'", screenshots};
    }

    TEST_PASS();
}

REGISTER_TEST(left_right_navigation, test_left_right_navigation);
REGISTER_TEST(up_down_at_boundaries, test_up_down_at_boundaries);
REGISTER_TEST(up_at_first_line, test_up_at_first_line);
REGISTER_TEST(down_at_last_line, test_down_at_last_line);
REGISTER_TEST(navigation_after_typing, test_navigation_after_typing);
REGISTER_TEST(rapid_navigation, test_rapid_navigation);
REGISTER_TEST(home_end_navigation, test_home_end_navigation);
