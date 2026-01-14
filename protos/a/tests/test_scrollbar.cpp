#include "test_helpers.h"
#include <iostream>

// Test scrollbar click and drag functionality
// Bug: Scrollbar was rendered but not interactive

TestResult test_scrollbar_click_drag(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Create content longer than viewport to enable scrollbar
    std::string content = R"(# Scrollbar Test Document

## Section 1

This is paragraph one with some text to fill the space.

This is paragraph two with more content.

This is paragraph three.

## Section 2

This is paragraph four.

This is paragraph five with additional text.

This is paragraph six.

## Section 3

This is paragraph seven.

This is paragraph eight with more text to make it longer.

This is paragraph nine.

## Section 4

This is paragraph ten.

This is paragraph eleven.

This is paragraph twelve.

## Section 5

This is paragraph thirteen.

This is paragraph fourteen.

This is paragraph fifteen - bottom of document.
)";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scrollbar_test", 0));

    std::cout << "  Step 1: Loaded long document (should see scrollbar on right)" << std::endl;

    // Step 2: Click on scrollbar track near the middle to jump
    // Scrollbar is on right side: trackX = width - 7 - 3 = 790 for 800px width
    float scrollbarX = ctx.getWidth() - 5;  // Middle of scrollbar
    float trackMiddleY = ctx.getHeight() / 2.0f;  // Middle of track

    ctx.simulateClick(scrollbarX, trackMiddleY);
    for (int i = 0; i < 5; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scrollbar_test", 1));

    std::cout << "  Step 2: Clicked on scrollbar track at middle - should jump to middle of document" << std::endl;

    // Step 3: Click at top of track to scroll back to top
    float trackTopY = 50.0f;  // Near top of track (below toolbar)
    ctx.simulateClick(scrollbarX, trackTopY);
    for (int i = 0; i < 5; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scrollbar_test", 2));

    std::cout << "  Step 3: Clicked at top of scrollbar track - should jump near top" << std::endl;

    // Step 4: Test drag - click and hold on thumb, then drag down
    // First need to find where thumb is (near top after step 3)
    // Simulate press, move, release for drag
    float thumbStartY = 55.0f;  // Approximate thumb position near top

    // Press on thumb
    ctx.simulateMousePress(scrollbarX, thumbStartY);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Drag down 200 pixels
    ctx.simulateMouseMove(scrollbarX, thumbStartY + 200);
    for (int i = 0; i < 5; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scrollbar_test", 3));

    std::cout << "  Step 4: Dragged scrollbar thumb down 200px" << std::endl;

    // Release mouse
    ctx.simulateMouseRelease(scrollbarX, thumbStartY + 200);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("scrollbar_test", 4));

    std::cout << "  Step 5: Released mouse - scroll position should be maintained" << std::endl;

    TEST_PASS();
}

REGISTER_TEST(scrollbar_click_drag, test_scrollbar_click_drag);
