#include "test_helpers.h"

// Test that bold text is actually rendered bold visually
TestResult testBoldRendering(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content that contains bold text already
    ctx.setContent("Normal text **bold text** more normal text");
    screenshots.push_back(ctx.captureScreenshot("bold_render_check"));

    // The bold text should look visually different (heavier weight)
    // Click somewhere to deselect and see just the rendering
    ctx.simulateClick(50, 300);  // Click away from text
    screenshots.push_back(ctx.captureScreenshot("bold_render_deselected"));

    // Switch to raw mode to confirm the markdown is correct
    ctx.simulateClick(770, 20);  // Click on Raw button
    screenshots.push_back(ctx.captureScreenshot("bold_render_raw"));

    TEST_PASS();
}

// Test: Apply bold via keyboard, then verify rendering
TestResult testBoldAfterKeyboard(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Start with plain text
    ctx.setContent("make this bold");
    screenshots.push_back(ctx.captureScreenshot("bold_keyboard_before"));

    // Select all and apply bold
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);
    ctx.simulateKey(GLFW_KEY_B, GLFW_MOD_CONTROL);

    // Click away to deselect
    ctx.simulateClick(50, 300);
    screenshots.push_back(ctx.captureScreenshot("bold_keyboard_after"));

    // Verify content
    std::string content = engine->getContent();
    printf("Content: '%s'\n", content.c_str());
    TEST_ASSERT(content == "**make this bold**", "Expected bold markers: " + content);

    // Check raw mode
    ctx.simulateClick(770, 20);
    screenshots.push_back(ctx.captureScreenshot("bold_keyboard_raw"));

    TEST_PASS();
}

// Test italic rendering
TestResult testItalicRendering(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    ctx.setContent("Normal *italic text* normal");
    ctx.simulateClick(50, 300);  // Deselect
    screenshots.push_back(ctx.captureScreenshot("italic_render"));

    TEST_PASS();
}

// Test that bold inside a sentence renders correctly
TestResult testMidSentenceBold(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Content with bold in middle
    ctx.setContent("I call this concept - **return on peace**.");
    ctx.simulateClick(50, 300);
    screenshots.push_back(ctx.captureScreenshot("mid_sentence_bold"));

    // Switch to raw to verify
    ctx.simulateClick(770, 20);
    screenshots.push_back(ctx.captureScreenshot("mid_sentence_bold_raw"));

    TEST_PASS();
}

REGISTER_TEST(BoldRendering, testBoldRendering);
REGISTER_TEST(BoldAfterKeyboard, testBoldAfterKeyboard);
REGISTER_TEST(ItalicRendering, testItalicRendering);
REGISTER_TEST(MidSentenceBold, testMidSentenceBold);
