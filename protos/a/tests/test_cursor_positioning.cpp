#include "test_helpers.h"
#include "../src/engine/layout_objects.h"
#include "../src/engine/markdown_parser.h"
#include "../src/engine/layout_engine.h"
#include "../src/engine/text_buffer.h"
#include <iostream>
#include <cmath>

// Unit test for cursor line detection in wrapped text
TestResult test_cursor_line_detection(TestContext& ctx) {
    std::vector<std::string> screenshots;

    // Test 1: getLineForChar should return correct line for positions at line boundaries
    // Create a TextLayoutObject and manually set up lines to test the edge cases

    // We'll use the engine to render text and verify cursor positions
    Engine* engine = ctx.getEngine();

    // Set up content with text that will wrap
    // Use a narrow viewport to force wrapping
    std::string content = "Hello world this is some text that will wrap to multiple lines when rendered";
    engine->setContent(content);

    // Render at narrow width to force wrapping
    engine->render(200, 600);

    // Capture initial state
    screenshots.push_back(ctx.captureScreenshot("cursor_line_detection", 0));

    TEST_PASS();
}

// Test that cursor Y position is correct for different lines
TestResult test_cursor_y_multiline(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create multi-line content
    std::string content = "Line one\n\nLine three\n\nLine five";
    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Take screenshots with cursor at different positions
    // Position 0 - start of line 1
    ctx.simulateClick(50, 50);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("cursor_y_multiline", 1));

    // Click on line 3 (after empty line)
    ctx.simulateClick(50, 150);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("cursor_y_multiline", 2));

    TEST_PASS();
}

// Test cursor position at line wrap boundaries
TestResult test_cursor_at_wrap_boundary(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Content that will wrap at a specific point
    std::string content = "Short words here that will definitely wrap around to the next line at some point";
    engine->setContent(content);

    // Render at narrow width
    engine->render(300, 400);
    screenshots.push_back(ctx.captureScreenshot("cursor_wrap_boundary", 0));

    // Click at the end of first line (near the wrap point)
    ctx.simulateClick(290, 50);
    engine->render(300, 400);
    screenshots.push_back(ctx.captureScreenshot("cursor_wrap_boundary", 1));

    // Click at start of second line
    ctx.simulateClick(50, 80);
    engine->render(300, 400);
    screenshots.push_back(ctx.captureScreenshot("cursor_wrap_boundary", 2));

    TEST_PASS();
}

// Direct unit test for TextLayoutObject::getLineForChar
TestResult test_getLineForChar_boundaries(TestContext& ctx) {
    std::vector<std::string> screenshots;

    // Create a simple markdown document
    TextBuffer buffer;
    buffer.setText("word1 word2 word3 word4 word5 word6 word7 word8");

    MarkdownParser parser;
    auto objectTree = parser.parse(buffer);

    if (!objectTree) {
        return TestResult{"test_getLineForChar_boundaries", false, "Failed to parse markdown", {}};
    }

    LayoutEngine layoutEngine;
    // Note: We need font faces for proper layout, but we can test the line logic

    auto layoutTree = layoutEngine.createLayoutTree(objectTree.get());
    if (!layoutTree) {
        return TestResult{"test_getLineForChar_boundaries", false, "Failed to create layout tree", {}};
    }

    // Layout at a narrow width to force wrapping
    Size narrowSize(150, 400);
    layoutEngine.performLayout(layoutTree.get(), narrowSize);

    // Find the TextLayoutObject
    const LayoutObject* textLayout = nullptr;
    std::function<void(const LayoutObject*)> findText = [&](const LayoutObject* obj) {
        if (dynamic_cast<const TextLayoutObject*>(obj)) {
            textLayout = obj;
            return;
        }
        for (const auto& child : obj->getChildren()) {
            findText(child.get());
        }
    };
    findText(layoutTree.get());

    if (!textLayout) {
        return TestResult{"test_getLineForChar_boundaries", false, "No TextLayoutObject found", {}};
    }

    const auto* tlo = dynamic_cast<const TextLayoutObject*>(textLayout);
    const auto& lines = tlo->getLines();

    std::cout << "  Lines found: " << lines.size() << std::endl;
    for (size_t i = 0; i < lines.size(); i++) {
        std::cout << "    Line " << i << ": startChar=" << lines[i].startChar
                  << ", endChar=" << lines[i].endChar << std::endl;
    }

    // Test: position at endChar of line 0 should return line 0
    if (lines.size() >= 2) {
        int endOfLine0 = lines[0].endChar;
        int lineForEnd = tlo->getLineForChar(endOfLine0);

        std::cout << "  Position " << endOfLine0 << " (end of line 0) returns line " << lineForEnd << std::endl;

        // The position at endChar should be on that line (line 0), not the next one
        if (lineForEnd != 0) {
            return TestResult{"test_getLineForChar_boundaries", false,
                "Position at endChar of line 0 should return line 0, got line " + std::to_string(lineForEnd), {}};
        }

        // Test positions in gaps (if any exist)
        for (size_t i = 0; i + 1 < lines.size(); i++) {
            int endOfLineI = lines[i].endChar;
            int startOfNextLine = lines[i + 1].startChar;

            if (endOfLineI < startOfNextLine) {
                // There's a gap - test positions in the gap
                for (int pos = endOfLineI; pos < startOfNextLine; pos++) {
                    int lineForPos = tlo->getLineForChar(pos);
                    // Positions in the gap should belong to line i (the line that just ended)
                    if (lineForPos != static_cast<int>(i)) {
                        std::cout << "  BUG: Position " << pos << " in gap after line " << i
                                  << " returns line " << lineForPos << std::endl;
                        return TestResult{"test_getLineForChar_boundaries", false,
                            "Position in gap should return previous line", {}};
                    }
                }
            }
        }
    }

    std::cout << "  All boundary positions return correct lines" << std::endl;
    TEST_PASS();
}

REGISTER_TEST(cursor_line_detection, test_cursor_line_detection);
REGISTER_TEST(cursor_y_multiline, test_cursor_y_multiline);
REGISTER_TEST(cursor_at_wrap_boundary, test_cursor_at_wrap_boundary);
REGISTER_TEST(getLineForChar_boundaries, test_getLineForChar_boundaries);
