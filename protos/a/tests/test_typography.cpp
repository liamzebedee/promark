#include "test_helpers.h"
#include <iostream>
#include <fstream>
#include <sstream>

// Capture a screenshot of long document for typography evaluation
TestResult test_typography_long_document(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Load long document
    std::ifstream file("../resources/long-post.md");
    if (!file.is_open()) {
        return TestResult{"typography_long_document", false,
            "Could not open long-post.md", screenshots};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    screenshots.push_back(ctx.captureScreenshot("typography_long"));
    std::cout << "  Screenshot saved: typography evaluation" << std::endl;

    TEST_PASS();
}

REGISTER_TEST(typography_long_document, test_typography_long_document);
