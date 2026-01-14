#include "test_helpers.h"
#include <iostream>

// Test: Does DOWN work when starting from middle of line?

TestResult test_down_from_middle(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "AAAA\nBBBB\nCCCC";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_middle", 0));

    std::cout << "\n=== DOWN FROM MIDDLE ===" << std::endl;

    // Click at middle of line 1 (between 'AA' and 'AA')
    ctx.simulateClick(56, 86);
    engine->render(400, 300);

    // Type to find position
    ctx.simulateKey(GLFW_KEY_1);
    engine->render(400, 300);
    std::string after1 = engine->getContent();
    std::cout << "After middle-click+1: '" << after1 << "'" << std::endl;
    size_t pos1 = after1.find('1');
    std::cout << "  '1' at position: " << pos1 << std::endl;

    // Reset
    engine->setContent(content);
    engine->render(400, 300);
    ctx.simulateClick(56, 86);  // Same middle position
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_middle", 1));

    // Press DOWN
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_middle", 2));

    // Type marker
    ctx.simulateKey(GLFW_KEY_2);
    engine->render(400, 300);
    std::string after2 = engine->getContent();
    std::cout << "After DOWN+2: '" << after2 << "'" << std::endl;
    size_t pos2 = after2.find('2');
    std::cout << "  '2' at position: " << pos2 << std::endl;

    size_t nl1 = after2.find('\n');
    size_t nl2 = after2.find('\n', nl1 + 1);
    std::cout << "  First NL: " << nl1 << ", Second NL: " << nl2 << std::endl;

    if (pos2 <= nl1) {
        std::cout << "  FAILURE: '2' still on line 1!" << std::endl;
        return TestResult{"test_down_from_middle", false,
            "DOWN from middle didn't move. '2' at " + std::to_string(pos2) + " <= first NL " + std::to_string(nl1),
            screenshots};
    }

    std::cout << "  SUCCESS: '2' on line 2" << std::endl;
    TEST_PASS();
}

// Test: DOWN from near end of shorter line to longer line
TestResult test_down_short_to_long(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "AA\nBBBBBBBBBB\nCC";
    engine->setContent(content);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_short_long", 0));

    std::cout << "\n=== DOWN SHORT TO LONG ===" << std::endl;

    // Click at end of short line 1
    ctx.simulateClick(56, 86);  // Near end of "AA"
    engine->render(400, 300);

    ctx.simulateKey(GLFW_KEY_1);
    engine->render(400, 300);
    std::string after1 = engine->getContent();
    std::cout << "After end-click+1: '" << after1 << "'" << std::endl;
    size_t pos1 = after1.find('1');
    std::cout << "  '1' at position: " << pos1 << std::endl;

    // Reset
    engine->setContent(content);
    engine->render(400, 300);
    ctx.simulateClick(56, 86);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_short_long", 1));

    // Press DOWN
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("nav_short_long", 2));

    ctx.simulateKey(GLFW_KEY_2);
    engine->render(400, 300);
    std::string after2 = engine->getContent();
    std::cout << "After DOWN+2: '" << after2 << "'" << std::endl;
    size_t pos2 = after2.find('2');
    std::cout << "  '2' at position: " << pos2 << std::endl;

    size_t nl1 = after2.find('\n');
    size_t nl2 = after2.find('\n', nl1 + 1);
    std::cout << "  First NL: " << nl1 << ", Second NL: " << nl2 << std::endl;

    if (pos2 <= nl1) {
        std::cout << "  FAILURE: '2' still on line 1!" << std::endl;
        return TestResult{"test_down_short_to_long", false,
            "DOWN from short to long didn't move. '2' at " + std::to_string(pos2),
            screenshots};
    }

    std::cout << "  SUCCESS: '2' on line 2" << std::endl;
    TEST_PASS();
}

REGISTER_TEST(down_from_middle, test_down_from_middle);
REGISTER_TEST(down_short_to_long, test_down_short_to_long);
