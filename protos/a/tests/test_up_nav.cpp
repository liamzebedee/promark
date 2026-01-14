#include "test_helpers.h"
#include <iostream>

// Focused test on UP navigation

TestResult test_up_simple(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "AAA\nBBB";
    engine->setContent(content);
    engine->render(400, 300);

    std::cout << "\n=== UP SIMPLE TEST ===" << std::endl;
    std::cout << "Content: '" << content << "'" << std::endl;

    // Click on line 2
    ctx.simulateClick(32, 102);  // Line 2
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("up_simple", 0));

    // Mark position on line 2
    ctx.simulateKey(GLFW_KEY_1);
    engine->render(400, 300);
    std::string after1 = engine->getContent();
    size_t pos1 = after1.find('1');
    std::cout << "After click L2 + '1': '" << after1 << "' pos=" << pos1 << std::endl;

    // Reset
    engine->setContent(content);
    engine->render(400, 300);

    // Click on line 2 again
    ctx.simulateClick(32, 102);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("up_simple", 1));

    // Press UP
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);
    screenshots.push_back(ctx.captureScreenshot("up_simple", 2));

    // Mark position
    ctx.simulateKey(GLFW_KEY_2);
    engine->render(400, 300);
    std::string after2 = engine->getContent();
    size_t pos2 = after2.find('2');
    size_t nl = after2.find('\n');
    std::cout << "After UP + '2': '" << after2 << "' pos=" << pos2 << " nl=" << nl << std::endl;

    if (pos2 > nl) {
        std::cout << "  FAIL: '2' is on line 2 (pos " << pos2 << " > nl " << nl << ")" << std::endl;
        return TestResult{"test_up_simple", false,
            "UP didn't move to line 1. '2' at " + std::to_string(pos2) +
            " > newline at " + std::to_string(nl),
            screenshots};
    }

    std::cout << "  OK: '2' is on line 1" << std::endl;
    TEST_PASS();
}

TestResult test_up_from_middle_of_line2(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "AAAA\nBBBB";
    engine->setContent(content);
    engine->render(400, 300);

    std::cout << "\n=== UP FROM MIDDLE OF LINE 2 ===" << std::endl;

    // Click in middle of line 2 (x=56)
    ctx.simulateClick(56, 102);
    engine->render(400, 300);

    ctx.simulateKey(GLFW_KEY_1);
    engine->render(400, 300);
    std::string after1 = engine->getContent();
    size_t pos1 = after1.find('1');
    std::cout << "After middle-click L2 + '1': '" << after1 << "' pos=" << pos1 << std::endl;

    // Reset
    engine->setContent(content);
    engine->render(400, 300);

    // Click in middle of line 2
    ctx.simulateClick(56, 102);
    engine->render(400, 300);

    // Press UP
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(400, 300);

    ctx.simulateKey(GLFW_KEY_2);
    engine->render(400, 300);
    std::string after2 = engine->getContent();
    size_t pos2 = after2.find('2');
    size_t nl = after2.find('\n');
    std::cout << "After UP + '2': '" << after2 << "' pos=" << pos2 << " nl=" << nl << std::endl;

    if (pos2 > nl) {
        std::cout << "  FAIL: '2' is on line 2" << std::endl;
        return TestResult{"test_up_from_middle_of_line2", false,
            "UP from middle didn't move to line 1", screenshots};
    }

    std::cout << "  OK: '2' is on line 1" << std::endl;
    TEST_PASS();
}

REGISTER_TEST(up_simple, test_up_simple);
REGISTER_TEST(up_from_middle_of_line2, test_up_from_middle_of_line2);
