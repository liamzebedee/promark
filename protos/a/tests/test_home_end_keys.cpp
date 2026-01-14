#include "test_helpers.h"

// Helper to type a character
static void typeChar(TestContext& ctx, char c) {
    int key = 0;
    int mods = 0;

    if (c >= 'A' && c <= 'Z') {
        key = GLFW_KEY_A + (c - 'A');
        mods = GLFW_MOD_SHIFT;
    } else if (c >= 'a' && c <= 'z') {
        key = GLFW_KEY_A + (c - 'a');
    } else if (c >= '0' && c <= '9') {
        key = GLFW_KEY_0 + (c - '0');
    }

    if (key != 0) {
        ctx.simulateKey(key, mods);
    }
}

// Test Home key goes to line start (not document start)
TestResult testHomeKey(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content with multiple lines
    ctx.setContent("Line1\nLine2\nLine3");

    // Position cursor in middle of Line2 (position 9 = "Line1\nLin")
    // We'll click to position cursor, then press right 3 times to get to middle
    ctx.setCursorPosition(9);  // After "Lin" in "Line2"

    screenshots.push_back(ctx.captureScreenshot("home_before"));

    // Press Home - should go to start of Line2 (position 6)
    ctx.simulateKey(GLFW_KEY_HOME, 0);

    // Type 'X' to mark where cursor is
    typeChar(ctx, 'X');

    std::string content = engine->getContent();

    // If Home went to line start correctly: "Line1\nXLine2\nLine3"
    // If Home went to document start (bug): "XLine1\nLine2\nLine3"
    std::string expected = "Line1\nXLine2\nLine3";

    screenshots.push_back(ctx.captureScreenshot("home_after"));

    TEST_ASSERT(content == expected,
        "Home should go to line start, not document start.\n"
        "Expected: '" + expected + "'\n"
        "Got: '" + content + "'");

    TEST_PASS();
}

// Test End key goes to line end (not document end)
TestResult testEndKey(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content with multiple lines
    ctx.setContent("Line1\nLine2\nLine3");

    // Position cursor at start of Line2 (position 6)
    ctx.setCursorPosition(6);

    screenshots.push_back(ctx.captureScreenshot("end_before"));

    // Press End - should go to end of Line2 (position 11, before \n)
    ctx.simulateKey(GLFW_KEY_END, 0);

    // Type 'X' to mark where cursor is
    typeChar(ctx, 'X');

    std::string content = engine->getContent();

    // If End went to line end correctly: "Line1\nLine2X\nLine3"
    // If End went to document end (bug): "Line1\nLine2\nLine3X"
    std::string expected = "Line1\nLine2X\nLine3";

    screenshots.push_back(ctx.captureScreenshot("end_after"));

    TEST_ASSERT(content == expected,
        "End should go to line end, not document end.\n"
        "Expected: '" + expected + "'\n"
        "Got: '" + content + "'");

    TEST_PASS();
}

// Test Ctrl+Home goes to document start
TestResult testCtrlHome(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content with multiple lines
    ctx.setContent("Line1\nLine2\nLine3");

    // Position cursor in middle of Line2
    ctx.setCursorPosition(9);

    // Press Ctrl+Home - should go to document start (position 0)
    ctx.simulateKey(GLFW_KEY_HOME, GLFW_MOD_CONTROL);

    // Type 'X' to mark where cursor is
    typeChar(ctx, 'X');

    std::string content = engine->getContent();
    std::string expected = "XLine1\nLine2\nLine3";

    screenshots.push_back(ctx.captureScreenshot("ctrl_home"));

    TEST_ASSERT(content == expected,
        "Ctrl+Home should go to document start.\n"
        "Expected: '" + expected + "'\n"
        "Got: '" + content + "'");

    TEST_PASS();
}

// Test Ctrl+End goes to document end
TestResult testCtrlEnd(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content with multiple lines
    ctx.setContent("Line1\nLine2\nLine3");

    // Position cursor at start
    ctx.setCursorPosition(0);

    // Press Ctrl+End - should go to document end
    ctx.simulateKey(GLFW_KEY_END, GLFW_MOD_CONTROL);

    // Type 'X' to mark where cursor is
    typeChar(ctx, 'X');

    std::string content = engine->getContent();
    std::string expected = "Line1\nLine2\nLine3X";

    screenshots.push_back(ctx.captureScreenshot("ctrl_end"));

    TEST_ASSERT(content == expected,
        "Ctrl+End should go to document end.\n"
        "Expected: '" + expected + "'\n"
        "Got: '" + content + "'");

    TEST_PASS();
}

// Test Shift+Home selects to line start
TestResult testShiftHome(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content with multiple lines
    ctx.setContent("Line1\nLine2\nLine3");

    // Position cursor in middle of Line2 (position 9)
    ctx.setCursorPosition(9);

    // Press Shift+Home - should select from position 9 to position 6
    ctx.simulateKey(GLFW_KEY_HOME, GLFW_MOD_SHIFT);

    // Check what is selected
    std::string selected = engine->getSelectedText();

    // Should select "Lin" (from start of Line2 to cursor position)
    std::string expected = "Lin";

    screenshots.push_back(ctx.captureScreenshot("shift_home"));

    TEST_ASSERT(selected == expected,
        "Shift+Home should select to line start.\n"
        "Expected selected: '" + expected + "'\n"
        "Got: '" + selected + "'");

    TEST_PASS();
}

// Test Shift+End selects to line end
TestResult testShiftEnd(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Set content with multiple lines
    ctx.setContent("Line1\nLine2\nLine3");

    // Position cursor at start of Line2 (position 6)
    ctx.setCursorPosition(6);

    // Press Shift+End - should select from position 6 to position 11
    ctx.simulateKey(GLFW_KEY_END, GLFW_MOD_SHIFT);

    // Check what is selected
    std::string selected = engine->getSelectedText();

    // Should select "Line2" (from cursor to end of line)
    std::string expected = "Line2";

    screenshots.push_back(ctx.captureScreenshot("shift_end"));

    TEST_ASSERT(selected == expected,
        "Shift+End should select to line end.\n"
        "Expected selected: '" + expected + "'\n"
        "Got: '" + selected + "'");

    TEST_PASS();
}

// Test Home on first line goes to position 0
TestResult testHomeFirstLine(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    ctx.setContent("Line1\nLine2\nLine3");

    // Position cursor in middle of first line
    ctx.setCursorPosition(3);  // "Lin|e1"

    ctx.simulateKey(GLFW_KEY_HOME, 0);

    typeChar(ctx, 'X');

    std::string content = engine->getContent();
    std::string expected = "XLine1\nLine2\nLine3";

    screenshots.push_back(ctx.captureScreenshot("home_first_line"));

    TEST_ASSERT(content == expected,
        "Home on first line should go to position 0.\n"
        "Expected: '" + expected + "'\n"
        "Got: '" + content + "'");

    TEST_PASS();
}

// Test End on last line goes to document end
TestResult testEndLastLine(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    ctx.setContent("Line1\nLine2\nLine3");

    // Position cursor at start of last line (position 12)
    ctx.setCursorPosition(12);

    ctx.simulateKey(GLFW_KEY_END, 0);

    typeChar(ctx, 'X');

    std::string content = engine->getContent();
    std::string expected = "Line1\nLine2\nLine3X";

    screenshots.push_back(ctx.captureScreenshot("end_last_line"));

    TEST_ASSERT(content == expected,
        "End on last line should go to document end.\n"
        "Expected: '" + expected + "'\n"
        "Got: '" + content + "'");

    TEST_PASS();
}

REGISTER_TEST(HomeKey, testHomeKey);
REGISTER_TEST(EndKey, testEndKey);
REGISTER_TEST(CtrlHome, testCtrlHome);
REGISTER_TEST(CtrlEnd, testCtrlEnd);
REGISTER_TEST(ShiftHome, testShiftHome);
REGISTER_TEST(ShiftEnd, testShiftEnd);
REGISTER_TEST(HomeFirstLine, testHomeFirstLine);
REGISTER_TEST(EndLastLine, testEndLastLine);
