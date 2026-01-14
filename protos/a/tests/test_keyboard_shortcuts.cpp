#include "test_helpers.h"

// Test Ctrl+X (Cut Selection)
TestResult testCutSelection(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content and select all
    ctx.setContent("Hello World");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);  // Select all

    // Verify selection
    std::string selected = engine->getSelectedText();
    TEST_ASSERT(selected == "Hello World", "Expected 'Hello World' to be selected, got: " + selected);

    // Cut
    ctx.simulateKey(GLFW_KEY_X, GLFW_MOD_CONTROL);

    // Verify content is now empty
    std::string content = engine->getContent();
    TEST_ASSERT(content.empty(), "Expected empty content after cut, got: " + content);

    // Paste to verify clipboard has the cut content
    ctx.simulateKey(GLFW_KEY_V, GLFW_MOD_CONTROL);
    content = engine->getContent();
    TEST_ASSERT(content == "Hello World", "Expected 'Hello World' after paste, got: " + content);

    screenshots.push_back(ctx.captureScreenshot("cut_selection"));
    TEST_PASS();
}

// Test Ctrl+B (Bold)
TestResult testBoldShortcut(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Test 1: Bold with selection
    ctx.setContent("Hello World");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);  // Select all
    ctx.simulateKey(GLFW_KEY_B, GLFW_MOD_CONTROL);  // Apply bold

    std::string content = engine->getContent();
    TEST_ASSERT(content == "**Hello World**", "Expected '**Hello World**', got: " + content);

    screenshots.push_back(ctx.captureScreenshot("bold_with_selection"));

    // Test 2: Bold without selection (insert markers)
    ctx.setContent("test");
    // Move cursor to end
    ctx.simulateKey(GLFW_KEY_DOWN, GLFW_MOD_CONTROL);  // Go to end
    ctx.simulateKey(GLFW_KEY_B, GLFW_MOD_CONTROL);  // Apply bold

    content = engine->getContent();
    TEST_ASSERT(content == "test****", "Expected 'test****', got: " + content);

    screenshots.push_back(ctx.captureScreenshot("bold_no_selection"));
    TEST_PASS();
}

// Test Ctrl+I (Italic)
TestResult testItalicShortcut(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Test 1: Italic with selection
    ctx.setContent("Hello World");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);  // Select all
    ctx.simulateKey(GLFW_KEY_I, GLFW_MOD_CONTROL);  // Apply italic

    std::string content = engine->getContent();
    TEST_ASSERT(content == "*Hello World*", "Expected '*Hello World*', got: " + content);

    screenshots.push_back(ctx.captureScreenshot("italic_with_selection"));

    // Test 2: Italic without selection (insert markers)
    ctx.setContent("test");
    ctx.simulateKey(GLFW_KEY_DOWN, GLFW_MOD_CONTROL);  // Go to end
    ctx.simulateKey(GLFW_KEY_I, GLFW_MOD_CONTROL);  // Apply italic

    content = engine->getContent();
    TEST_ASSERT(content == "test**", "Expected 'test**', got: " + content);

    screenshots.push_back(ctx.captureScreenshot("italic_no_selection"));
    TEST_PASS();
}

// Test Ctrl+K (Link)
TestResult testLinkShortcut(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Test 1: Link with selection
    ctx.setContent("example");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);  // Select all
    ctx.simulateKey(GLFW_KEY_K, GLFW_MOD_CONTROL);  // Apply link

    std::string content = engine->getContent();
    TEST_ASSERT(content == "[example](url)", "Expected '[example](url)', got: " + content);

    // Verify "url" is selected for easy replacement
    std::string selected = engine->getSelectedText();
    TEST_ASSERT(selected == "url", "Expected 'url' to be selected, got: " + selected);

    screenshots.push_back(ctx.captureScreenshot("link_with_selection"));

    // Test 2: Link without selection
    ctx.setContent("before ");
    ctx.simulateKey(GLFW_KEY_DOWN, GLFW_MOD_CONTROL);  // Go to end
    ctx.simulateKey(GLFW_KEY_K, GLFW_MOD_CONTROL);  // Apply link

    content = engine->getContent();
    TEST_ASSERT(content == "before [text](url)", "Expected 'before [text](url)', got: " + content);

    // Verify "text" is selected for easy replacement
    selected = engine->getSelectedText();
    TEST_ASSERT(selected == "text", "Expected 'text' to be selected, got: " + selected);

    screenshots.push_back(ctx.captureScreenshot("link_no_selection"));
    TEST_PASS();
}

// Test Ctrl+` (Inline Code)
TestResult testInlineCodeShortcut(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Test 1: Inline code with selection
    ctx.setContent("code");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);  // Select all
    ctx.simulateKey(GLFW_KEY_GRAVE_ACCENT, GLFW_MOD_CONTROL);  // Apply inline code

    std::string content = engine->getContent();
    TEST_ASSERT(content == "`code`", "Expected '`code`', got: " + content);

    screenshots.push_back(ctx.captureScreenshot("inline_code_with_selection"));

    // Test 2: Inline code without selection
    ctx.setContent("test");
    ctx.simulateKey(GLFW_KEY_DOWN, GLFW_MOD_CONTROL);  // Go to end
    ctx.simulateKey(GLFW_KEY_GRAVE_ACCENT, GLFW_MOD_CONTROL);  // Apply inline code

    content = engine->getContent();
    TEST_ASSERT(content == "test``", "Expected 'test``', got: " + content);

    screenshots.push_back(ctx.captureScreenshot("inline_code_no_selection"));
    TEST_PASS();
}

// Test Ctrl+X with no selection (should do nothing)
TestResult testCutNoSelection(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    ctx.setContent("Hello World");
    // Don't select anything

    // Try to cut
    ctx.simulateKey(GLFW_KEY_X, GLFW_MOD_CONTROL);

    // Content should be unchanged
    std::string content = engine->getContent();
    TEST_ASSERT(content == "Hello World", "Content should be unchanged when cutting with no selection");

    screenshots.push_back(ctx.captureScreenshot("cut_no_selection"));
    TEST_PASS();
}

// Test cut is undoable
TestResult testCutUndo(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    ctx.setContent("Hello World");
    ctx.simulateKey(GLFW_KEY_A, GLFW_MOD_CONTROL);  // Select all
    ctx.simulateKey(GLFW_KEY_X, GLFW_MOD_CONTROL);  // Cut

    // Content should be empty
    std::string content = engine->getContent();
    TEST_ASSERT(content.empty(), "Content should be empty after cut");

    // Undo
    ctx.simulateKey(GLFW_KEY_Z, GLFW_MOD_CONTROL);

    // Content should be restored
    content = engine->getContent();
    TEST_ASSERT(content == "Hello World", "Expected 'Hello World' after undo, got: " + content);

    screenshots.push_back(ctx.captureScreenshot("cut_undo"));
    TEST_PASS();
}

REGISTER_TEST(CutSelection, testCutSelection);
REGISTER_TEST(BoldShortcut, testBoldShortcut);
REGISTER_TEST(ItalicShortcut, testItalicShortcut);
REGISTER_TEST(LinkShortcut, testLinkShortcut);
REGISTER_TEST(InlineCodeShortcut, testInlineCodeShortcut);
REGISTER_TEST(CutNoSelection, testCutNoSelection);
REGISTER_TEST(CutUndo, testCutUndo);
