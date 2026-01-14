#include "test_helpers.h"
#include "../src/engine/markdown_renderer.h"
#include "../src/engine/typography.h"
#include <iostream>
#include <cmath>

// Trace test to understand exactly what's happening in vertical navigation

TestResult test_navigation_trace(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "AAA\nBBB\nCCC";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_trace", 0));

    std::cout << "\n=== NAVIGATION TRACE ===" << std::endl;
    std::cout << "Content: '" << content << "'" << std::endl;

    // Click at start of line 1
    // Line 1's hit region is around y=35.2-51.2, so y=40 is safe
    ctx.simulateClick(32, 80);  // Left margin, first line
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_trace", 1));

    // Type a marker
    ctx.simulateKey(GLFW_KEY_1);
    engine->render(400, 300);
    std::string after1 = engine->getContent();
    std::cout << "After click+1: '" << after1 << "'" << std::endl;

    // Find where '1' is
    size_t pos1 = after1.find('1');
    std::cout << "  '1' at position: " << pos1 << std::endl;

    // Reset
    engine->setContent(content);
    engine->render(400, 300);

    // Click at same position (line 1)
    // Use slightly different x to avoid double-click detection (same y, different x)
    ctx.simulateClick(40, 80);
    engine->render(400, 300);

    // Now press Down
    std::cout << "\nPressing DOWN..." << std::endl;
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_trace", 2));

    // Type marker
    ctx.simulateKey(GLFW_KEY_2);
    engine->render(400, 300);
    std::string after2 = engine->getContent();
    std::cout << "After DOWN+2: '" << after2 << "'" << std::endl;

    size_t pos2 = after2.find('2');
    std::cout << "  '2' at position: " << pos2 << std::endl;

    // Analyze
    size_t nl1 = after2.find('\n');
    size_t nl2 = after2.find('\n', nl1 + 1);
    std::cout << "  First newline: " << nl1 << std::endl;
    std::cout << "  Second newline: " << nl2 << std::endl;

    if (pos2 != std::string::npos && pos2 > nl1 && pos2 < nl2) {
        std::cout << "  SUCCESS: '2' is on line 2" << std::endl;
    } else if (pos2 != std::string::npos && pos2 <= nl1) {
        std::cout << "  FAILURE: '2' is on line 1 (DOWN didn't move!)" << std::endl;
        return TestResult{"test_navigation_trace", false,
            "DOWN didn't move cursor to line 2. '2' at " + std::to_string(pos2),
            screenshots};
    } else if (pos2 != std::string::npos && pos2 > nl2) {
        std::cout << "  FAILURE: '2' jumped to line 3" << std::endl;
        return TestResult{"test_navigation_trace", false,
            "DOWN jumped to line 3. '2' at " + std::to_string(pos2),
            screenshots};
    }

    TEST_PASS();
}

REGISTER_TEST(navigation_trace, test_navigation_trace);
