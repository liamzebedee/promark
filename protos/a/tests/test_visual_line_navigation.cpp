#include "test_helpers.h"
#include "../src/engine/layout_objects.h"
#include "../src/engine/markdown_parser.h"
#include "../src/engine/layout_engine.h"
#include "../src/engine/text_buffer.h"
#include <iostream>
#include <cmath>

// Test that Up/Down navigate by visual line, not logical line
// Bug: Shift+Up selects entire paragraph instead of extending selection by one visual line
TestResult test_visual_line_navigation_up(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create content that will wrap to MANY visual lines (5+)
    // This is a single paragraph (no newlines) - narrow viewport forces wrapping
    std::string content = "This is a very long paragraph that will definitely wrap to many many visual lines when rendered in a narrow viewport of only 200 pixels wide. We need enough text here to guarantee at least five or six visual lines so we can properly test the Up arrow key navigation behavior within a single logical paragraph.";
    engine->setContent(content);

    // Render at very narrow width to force many lines of wrapping
    int narrowWidth = 200;
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("visual_line_nav_up", 0));

    // Click on line 4 or 5 (well below the first line)
    // With toolbar at 40px and ~20px line height, y=140 should be ~line 5
    ctx.simulateClick(50, 140);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("visual_line_nav_up", 1));

    // Now press Up - this should move UP by one visual line, staying in the same paragraph
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("visual_line_nav_up", 2));

    // Press Up again - should move to another visual line above
    ctx.simulateKey(GLFW_KEY_UP);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("visual_line_nav_up", 3));

    // The cursor should still be in the same paragraph but moved up by visual lines
    // If the bug exists, cursor would have jumped to position 0 on first Up press

    TEST_PASS();
}

// Test Shift+Up extends selection by visual line
TestResult test_shift_up_visual_line_selection(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create a very long paragraph that wraps to many lines
    std::string content = "This is a very long paragraph that will definitely wrap to many many visual lines when rendered in a narrow viewport of only 200 pixels wide. We need enough text here to guarantee at least five or six visual lines so we can properly test the Shift+Up selection behavior within a single logical paragraph that has no newlines.";
    engine->setContent(content);

    int narrowWidth = 200;
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("shift_up_selection", 0));

    // Click on visual line 3 (third line of wrapped text)
    // Toolbar=40px, margin=32px, line height=16px
    // Line 1: y=72-88, Line 2: y=88-104, Line 3: y=104-120, Line 4: y=120-136
    // Click at y=112 should be in line 3
    ctx.simulateClick(100, 112);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("shift_up_selection", 1));

    // Press Shift+Up - should select from current position up to the previous visual line
    ctx.simulateKey(GLFW_KEY_UP, GLFW_MOD_SHIFT);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("shift_up_selection", 2));

    // Get the selected text
    std::string selectedText = engine->getSelectedText();
    std::cout << "  Selected text length: " << selectedText.length() << std::endl;
    if (selectedText.length() > 0) {
        std::cout << "  Selected text: \"" << selectedText.substr(0, std::min((size_t)50, selectedText.length())) << "...\"" << std::endl;
    } else {
        std::cout << "  Selected text: (empty)" << std::endl;
    }

    // If the bug exists, selectedText will be very long (entire paragraph = ~310 chars)
    // If fixed, selectedText should be roughly one visual line's worth of text
    // At 200px width with margins, each visual line is ~80-100 characters
    // Total paragraph is ~310 chars over 4 lines
    size_t totalLen = content.length();
    size_t maxExpected = totalLen * 2 / 3;  // More than 2/3 of paragraph = bug

    if (selectedText.length() > maxExpected) {
        return TestResult{"test_shift_up_visual_line_selection", false,
            "Shift+Up selected " + std::to_string(selectedText.length()) +
            " chars (>" + std::to_string(maxExpected) +
            ") - should be one visual line, not most of the paragraph",
            screenshots};
    }

    // Also check that something WAS selected (the bug might cause empty selection too)
    if (selectedText.length() == 0) {
        return TestResult{"test_shift_up_visual_line_selection", false,
            "Shift+Up selected nothing - should select one visual line of text",
            screenshots};
    }

    // Reasonable range for one visual line: 30-150 characters
    // (depends on exact font metrics and line breaking)
    if (selectedText.length() < 30) {
        return TestResult{"test_shift_up_visual_line_selection", false,
            "Shift+Up selected only " + std::to_string(selectedText.length()) +
            " chars - should be approximately one visual line (30+ chars)",
            screenshots};
    }

    TEST_PASS();
}

// Test Down navigation by visual line
TestResult test_visual_line_navigation_down(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create content that wraps
    std::string content = "This is a long paragraph that should wrap to multiple visual lines. We test that Down moves by visual line within a wrapped paragraph.";
    engine->setContent(content);

    int narrowWidth = 300;
    engine->render(narrowWidth, 400);

    // Click at the start of the content
    ctx.simulateClick(50, 50);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("visual_line_nav_down", 0));

    // Press Down - should move to next visual line, still in same paragraph
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("visual_line_nav_down", 1));

    // Press Down again
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("visual_line_nav_down", 2));

    TEST_PASS();
}

// Test that goal column is preserved during vertical navigation
TestResult test_goal_column_preservation(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create content with different line lengths
    std::string content = "Short line\nThis is a much longer line with more text that wraps\nShort again";
    engine->setContent(content);

    int narrowWidth = 300;
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("goal_column", 0));

    // Click near the end of the first line
    ctx.simulateClick(80, 50);  // Near "line" on first line
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("goal_column", 1));

    // Move down - should try to preserve x position
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("goal_column", 2));

    // Move down again (might wrap or go to third line)
    ctx.simulateKey(GLFW_KEY_DOWN);
    engine->render(narrowWidth, 400);
    screenshots.push_back(ctx.captureScreenshot("goal_column", 3));

    TEST_PASS();
}

REGISTER_TEST(visual_line_navigation_up, test_visual_line_navigation_up);
REGISTER_TEST(shift_up_visual_line_selection, test_shift_up_visual_line_selection);
REGISTER_TEST(visual_line_navigation_down, test_visual_line_navigation_down);
REGISTER_TEST(goal_column_preservation, test_goal_column_preservation);
