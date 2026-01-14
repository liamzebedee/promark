#include "test_helpers.h"

// Test Ctrl+B visually to see what the bug report describes
TestResult testBoldFormattingVisual(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content similar to the bug report scenario
    ctx.setContent("One of the ways to approach life is through return on peace.");

    // Capture initial state
    screenshots.push_back(ctx.captureScreenshot("bold_visual_initial"));

    // Select "return on peace" by clicking and dragging
    // First, click at position where "return" starts
    // Position ~character 45 which is roughly x=320 at 16px font
    ctx.simulateMousePress(300, 80);  // Approximate start of "return"
    ctx.simulateMouseMove(430, 80);   // Approximate end of "peace"
    ctx.simulateMouseRelease(430, 80);

    // Capture selection state
    screenshots.push_back(ctx.captureScreenshot("bold_visual_selected"));

    std::string selected = engine->getSelectedText();
    // Show what was selected
    printf("Selected text: '%s'\n", selected.c_str());

    // Apply bold
    ctx.simulateKey(GLFW_KEY_B, GLFW_MOD_CONTROL);

    // Capture after bold applied
    screenshots.push_back(ctx.captureScreenshot("bold_visual_after_bold"));

    std::string content = engine->getContent();
    printf("Content after bold: '%s'\n", content.c_str());

    // Switch to raw mode to see the actual markdown
    ctx.simulateClick(770, 20);  // Click on Raw button (approximate position)
    screenshots.push_back(ctx.captureScreenshot("bold_visual_raw_mode"));

    TEST_PASS();
}

// Test Ctrl+B in the middle of a line to detect the "vertical divider" issue
TestResult testBoldMidLine(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set some multi-line content
    ctx.setContent("Hello World\n\nThis is a test paragraph with some text.\n\nAnother paragraph here.");

    // Capture initial state
    screenshots.push_back(ctx.captureScreenshot("bold_midline_initial"));

    // Select "test" by clicking and using keyboard shift+arrow
    // Let's use select all first for a simple case
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);  // Select all

    // Capture selection
    screenshots.push_back(ctx.captureScreenshot("bold_midline_selected"));

    // Apply bold
    ctx.simulateKey(GLFW_KEY_B, GLFW_MOD_CONTROL);

    // Capture after bold
    screenshots.push_back(ctx.captureScreenshot("bold_midline_after"));

    std::string content = engine->getContent();
    printf("Content after bold: '%s'\n", content.c_str());

    TEST_PASS();
}

// Test multiple formatting operations to look for anomalies
TestResult testFormattingVariations(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Test 1: Bold a single word
    ctx.setContent("word");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);  // Select all
    ctx.simulateKey(GLFW_KEY_B, GLFW_MOD_CONTROL);  // Bold
    screenshots.push_back(ctx.captureScreenshot("fmt_bold_single_word"));

    std::string content = engine->getContent();
    printf("Bold single word: '%s'\n", content.c_str());
    TEST_ASSERT(content == "**word**", "Bold single word failed: " + content);

    // Test 2: Bold without selection (should insert ****)
    ctx.setContent("before ");
    ctx.simulateKey(GLFW_KEY_END, 0);  // Go to end
    ctx.simulateKey(GLFW_KEY_B, GLFW_MOD_CONTROL);  // Bold with no selection
    screenshots.push_back(ctx.captureScreenshot("fmt_bold_no_selection"));

    content = engine->getContent();
    printf("Bold no selection: '%s'\n", content.c_str());
    TEST_ASSERT(content == "before ****", "Bold no selection failed: " + content);

    // Test 3: Italic
    ctx.setContent("italic text");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);
    ctx.simulateKey(GLFW_KEY_I, GLFW_MOD_CONTROL);
    screenshots.push_back(ctx.captureScreenshot("fmt_italic"));

    content = engine->getContent();
    printf("Italic: '%s'\n", content.c_str());
    TEST_ASSERT(content == "*italic text*", "Italic failed: " + content);

    // Test 4: Inline code
    ctx.setContent("code here");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);
    ctx.simulateKey(GLFW_KEY_GRAVE_ACCENT, GLFW_MOD_CONTROL);
    screenshots.push_back(ctx.captureScreenshot("fmt_code"));

    content = engine->getContent();
    printf("Inline code: '%s'\n", content.c_str());
    TEST_ASSERT(content == "`code here`", "Inline code failed: " + content);

    // Test 5: Link
    ctx.setContent("link text");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);
    ctx.simulateKey(GLFW_KEY_K, GLFW_MOD_CONTROL);
    screenshots.push_back(ctx.captureScreenshot("fmt_link"));

    content = engine->getContent();
    printf("Link: '%s'\n", content.c_str());
    TEST_ASSERT(content == "[link text](url)", "Link failed: " + content);

    TEST_PASS();
}

REGISTER_TEST(BoldFormattingVisual, testBoldFormattingVisual);
REGISTER_TEST(BoldMidLine, testBoldMidLine);
REGISTER_TEST(FormattingVariations, testFormattingVariations);
