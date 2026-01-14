#include "test_helpers.h"
#include <iostream>

// Test that caret is visible inside blockquotes
TestResult test_caret_in_blockquote(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create content with a blockquote
    std::string content = "Normal paragraph.\n\n> This is a blockquote with some text inside it.\n\nAnother paragraph.";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("blockquote_caret", 0));

    // Click inside the blockquote text
    // Blockquote starts at y around 130+ (after first paragraph and empty line)
    ctx.simulateClick(150, 140);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("blockquote_caret", 1));

    // Type a character to verify cursor is at correct position
    engine->handleKeyboard('X', 0, GLFW_PRESS, 0);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("blockquote_caret", 2));

    // Check that the content was modified (meaning cursor was positioned correctly)
    std::string modified = engine->getContent();
    // handleKeyboard with 'X' inserts lowercase 'x'
    if (modified.find("x") == std::string::npos) {
        return TestResult{"test_caret_in_blockquote", false,
            "Character was not inserted - cursor may not be visible", screenshots};
    }

    // Verify the 'x' was inserted inside the blockquote (not before/after it)
    size_t xPos = modified.find("x");
    size_t blockquoteStart = modified.find(">");
    size_t blockquoteEnd = modified.find("\n\nAnother");
    if (xPos < blockquoteStart || xPos > blockquoteEnd) {
        return TestResult{"test_caret_in_blockquote", false,
            "Character was inserted outside blockquote - click position may be wrong", screenshots};
    }

    // Debug: print cursor position info
    int cursorPos = engine->getCursorPosition();
    std::cout << "  Cursor position after click: " << cursorPos << std::endl;
    std::cout << "  Content: " << modified.substr(0, 100) << "..." << std::endl;

    TEST_PASS();
}

// Test that selection highlight is visible inside blockquotes
TestResult test_selection_in_blockquote(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create content with a blockquote
    std::string content = "Normal paragraph.\n\n> This is a blockquote with some text inside it.\n\nAnother paragraph.";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("blockquote_selection", 0));

    // Click inside the blockquote text
    ctx.simulateClick(150, 140);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("blockquote_selection", 1));

    // Select some text with Shift+Right
    for (int i = 0; i < 10; i++) {
        ctx.simulateKey(GLFW_KEY_RIGHT, GLFW_MOD_SHIFT);
    }
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("blockquote_selection", 2));

    // Check that text was selected
    std::string selected = engine->getSelectedText();
    std::cout << "  Selected text: '" << selected << "'" << std::endl;
    if (selected.empty()) {
        return TestResult{"test_selection_in_blockquote", false,
            "No text was selected - selection may not work in blockquotes", screenshots};
    }

    TEST_PASS();
}

REGISTER_TEST(caret_in_blockquote, test_caret_in_blockquote);
REGISTER_TEST(selection_in_blockquote, test_selection_in_blockquote);
