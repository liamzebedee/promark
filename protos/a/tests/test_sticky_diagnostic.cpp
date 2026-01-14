#include "test_helpers.h"
#include "../src/engine/layout_objects.h"
#include "../src/engine/markdown_parser.h"
#include "../src/engine/layout_engine.h"
#include "../src/engine/text_buffer.h"
#include <iostream>
#include <cmath>

// Diagnostic test to understand the sticky caret behavior
// This test prints detailed information about cursor positions and movements

TestResult test_diagnostic_up_down(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Simple 3-line content
    std::string content = "AAA\nBBBBBBBBB\nCCC";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("diag_updown", 0));

    // Content layout:
    // Position 0-3: "AAA\n" (line 1, 3 chars + newline)
    // Position 4-13: "BBBBBBBBB\n" (line 2, 9 chars + newline)
    // Position 14-17: "CCC" (line 3, 3 chars)

    std::cout << "  Content: '" << content << "'" << std::endl;
    std::cout << "  Content length: " << content.length() << std::endl;

    // Click at middle of first line (around position 1-2)
    // Line 1's hit region is y=35.2-51.2, so use y=40
    ctx.simulateClick(48, 80);  // Small x, first line
    engine->render(400, 300);

    // Type a marker to find initial position
    ctx.simulateKey(GLFW_KEY_1);
    engine->render(400, 300);
    std::string afterClick = engine->getContent();
    size_t pos1 = afterClick.find('1');
    std::cout << "  After click and '1': pos=" << pos1 << ", content='" << afterClick << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("diag_updown", 1));

    // Reset and try again
    engine->setContent(content);
    engine->render(400, 300);

    // Click within first line (not past end)
    // Line 1's hit region is y=35.2-51.2 (content), lineX=32-62
    // Click at x=56 to be safely within line 1's bounds
    // Use different x than previous click to avoid double-click detection
    ctx.simulateClick(56, 80);  // Within "AAA"
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("diag_updown", 2));

    // Press Down - should go to line 2
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("diag_updown", 3));

    // Type marker
    ctx.simulateKey(GLFW_KEY_2);
    engine->render(400, 300);
    std::string afterDown = engine->getContent();
    size_t pos2 = afterDown.find('2');
    std::cout << "  After Down and '2': pos=" << pos2 << ", content='" << afterDown << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("diag_updown", 4));

    // Check if '2' is on line 2 (positions 4-12 after the '2' was inserted, original was 4-13)
    // After inserting '1' on line 1, positions shift, so we need to be careful
    // Let's just check if '2' is between the first and second newline
    size_t firstNL = afterDown.find('\n');
    size_t secondNL = afterDown.find('\n', firstNL + 1);

    std::cout << "  First newline at: " << firstNL << std::endl;
    std::cout << "  Second newline at: " << secondNL << std::endl;
    std::cout << "  '2' at: " << pos2 << std::endl;

    if (pos2 <= firstNL) {
        return TestResult{"test_diagnostic_up_down", false,
            "Down key didn't move to line 2. '2' is at pos " + std::to_string(pos2) +
            " which is on line 1 (before first newline at " + std::to_string(firstNL) + ")",
            screenshots};
    }

    if (pos2 > secondNL) {
        return TestResult{"test_diagnostic_up_down", false,
            "Down key moved past line 2. '2' is at pos " + std::to_string(pos2) +
            " which is after second newline at " + std::to_string(secondNL),
            screenshots};
    }

    std::cout << "  '2' correctly on line 2" << std::endl;

    // Now test Up from line 2
    engine->setContent(content);
    engine->render(400, 300);

    // Click at middle of line 2
    // Line 2's hit region is y=67.2-83.2, so use y=75
    ctx.simulateClick(64, 115);  // Line 2 y position
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("diag_updown", 5));

    // Press Up - should go to line 1
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("diag_updown", 6));

    // Type marker
    ctx.simulateKey(GLFW_KEY_3);
    engine->render(400, 300);
    std::string afterUp = engine->getContent();
    size_t pos3 = afterUp.find('3');
    std::cout << "  After Up and '3': pos=" << pos3 << ", content='" << afterUp << "'" << std::endl;
    screenshots.push_back(ctx.captureScreenshot("diag_updown", 7));

    // Check if '3' is on line 1 (before first newline)
    size_t firstNL2 = afterUp.find('\n');

    if (pos3 > firstNL2) {
        return TestResult{"test_diagnostic_up_down", false,
            "Up key didn't move to line 1. '3' is at pos " + std::to_string(pos3) +
            " which is after first newline at " + std::to_string(firstNL2),
            screenshots};
    }

    std::cout << "  '3' correctly on line 1" << std::endl;

    TEST_PASS();
}

// Test sequential up/down/up/down navigation
TestResult test_diagnostic_sequential_nav(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "LINE1\nLINE2\nLINE3";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("diag_seq", 0));

    std::cout << "  Content: '" << content << "'" << std::endl;

    // Start on line 1
    ctx.simulateClick(48, 80);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("diag_seq", 1));

    // Track positions through navigation
    std::vector<std::string> positions;

    // Down -> Line 2
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_A);
    engine->render(400, 300);
    positions.push_back(engine->getContent());
    screenshots.push_back(ctx.captureScreenshot("diag_seq", 2));

    // Reset
    engine->setContent(content);
    engine->render(400, 300);
    ctx.simulateClick(48, 80);
    engine->render(400, 300);

    // Down -> Down -> Line 3
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_B);
    engine->render(400, 300);
    positions.push_back(engine->getContent());
    screenshots.push_back(ctx.captureScreenshot("diag_seq", 3));

    // Reset
    engine->setContent(content);
    engine->render(400, 300);
    ctx.simulateClick(48, 80);
    engine->render(400, 300);

    // Down -> Down -> Up -> Line 2
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);
    ctx.simulateKey(GLFW_KEY_C);
    engine->render(400, 300);
    positions.push_back(engine->getContent());
    screenshots.push_back(ctx.captureScreenshot("diag_seq", 4));

    std::cout << "  After Down (from L1): '" << positions[0] << "'" << std::endl;
    std::cout << "  After Down-Down (from L1): '" << positions[1] << "'" << std::endl;
    std::cout << "  After Down-Down-Up (from L1): '" << positions[2] << "'" << std::endl;

    // Verify 'a' is on line 2
    size_t aPos = positions[0].find('a');
    size_t nl1_0 = positions[0].find('\n');
    size_t nl2_0 = positions[0].find('\n', nl1_0 + 1);
    if (aPos == std::string::npos || aPos <= nl1_0 || aPos > nl2_0) {
        return TestResult{"test_diagnostic_sequential_nav", false,
            "Down from L1 didn't reach L2. 'a' at " + std::to_string(aPos),
            screenshots};
    }

    // Verify 'b' is on line 3
    size_t bPos = positions[1].find('b');
    size_t nl1_1 = positions[1].find('\n');
    size_t nl2_1 = positions[1].find('\n', nl1_1 + 1);
    if (bPos == std::string::npos || bPos <= nl2_1) {
        return TestResult{"test_diagnostic_sequential_nav", false,
            "Down-Down from L1 didn't reach L3. 'b' at " + std::to_string(bPos),
            screenshots};
    }

    // Verify 'c' is on line 2
    size_t cPos = positions[2].find('c');
    size_t nl1_2 = positions[2].find('\n');
    size_t nl2_2 = positions[2].find('\n', nl1_2 + 1);
    if (cPos == std::string::npos || cPos <= nl1_2 || cPos > nl2_2) {
        return TestResult{"test_diagnostic_sequential_nav", false,
            "Down-Down-Up from L1 didn't reach L2. 'c' at " + std::to_string(cPos),
            screenshots};
    }

    std::cout << "  All sequential navigation tests passed!" << std::endl;

    TEST_PASS();
}

REGISTER_TEST(diagnostic_up_down, test_diagnostic_up_down);
REGISTER_TEST(diagnostic_sequential_nav, test_diagnostic_sequential_nav);
