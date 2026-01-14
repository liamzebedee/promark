#include "test_helpers.h"
#include <iostream>

// Test DOWN with different X click positions

TestResult test_down_x_positions(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    std::string content = "LINE1\nLINE2\nLINE3";
    std::cout << "\n=== DOWN WITH DIFFERENT X POSITIONS ===" << std::endl;
    std::cout << "Content: '" << content << "'" << std::endl;

    // Test click at different X positions
    std::vector<int> xPositions = {32, 40, 48, 56, 64, 80, 100};

    for (int clickX : xPositions) {
        engine->setContent(content);
        engine->render(400, 300);

        ctx.simulateClick(clickX, 86);
        engine->render(400, 300);

        // Mark position before DOWN
        ctx.simulateKey(GLFW_KEY_PERIOD);  // '.' marker
        engine->render(400, 300);
        std::string beforeDown = engine->getContent();

        // Reset and try DOWN
        engine->setContent(content);
        engine->render(400, 300);
        ctx.simulateClick(clickX, 86);
        engine->render(400, 300);

        ctx.simulateKey(GLFW_KEY_DOWN);
        engine->render(400, 300);

        ctx.simulateKey(GLFW_KEY_COMMA);  // ',' marker
        engine->render(400, 300);
        std::string afterDown = engine->getContent();

        size_t dotPos = beforeDown.find('.');
        size_t commaPos = afterDown.find(',');
        size_t nl1 = afterDown.find('\n');

        std::cout << "  x=" << clickX << ": ";
        std::cout << "before='" << beforeDown.substr(0, 10) << "' (pos=" << dotPos << "), ";
        std::cout << "after='" << afterDown.substr(0, 15) << "' (pos=" << commaPos << ") ";

        if (commaPos != std::string::npos && commaPos > nl1) {
            std::cout << "[OK - moved to L2]" << std::endl;
        } else if (commaPos != std::string::npos && commaPos <= nl1) {
            std::cout << "[FAIL - stayed on L1!]" << std::endl;
        }
    }

    screenshots.push_back(ctx.captureScreenshot("nav_x_pos", 0));

    TEST_PASS();
}

REGISTER_TEST(down_x_positions, test_down_x_positions);
