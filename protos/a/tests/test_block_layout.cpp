#include "test_helpers.h"

// Test block-level layout: lists, blockquotes, horizontal rules
TestResult testBlockLayout(TestContext& ctx) {
    std::vector<std::string> screenshots;

    // Test 1: Unordered lists
    ctx.setContent(
        "# Unordered Lists\n"
        "\n"
        "- First item\n"
        "- Second item\n"
        "- Third item\n"
        "\n"
        "Different bullet:\n"
        "\n"
        "* Item A\n"
        "* Item B\n"
        "* Item C\n"
    );
    screenshots.push_back(ctx.captureScreenshot("block_layout_unordered_list"));

    // Test 2: Ordered lists
    ctx.setContent(
        "# Ordered Lists\n"
        "\n"
        "1. First step\n"
        "2. Second step\n"
        "3. Third step\n"
        "\n"
        "Starting from different number:\n"
        "\n"
        "5. Fifth item\n"
        "6. Sixth item\n"
        "7. Seventh item\n"
    );
    screenshots.push_back(ctx.captureScreenshot("block_layout_ordered_list"));

    // Test 3: Nested lists
    ctx.setContent(
        "# Nested Lists\n"
        "\n"
        "- Parent item 1\n"
        "  - Child item 1.1\n"
        "  - Child item 1.2\n"
        "- Parent item 2\n"
        "  - Child item 2.1\n"
        "    - Grandchild 2.1.1\n"
        "  - Child item 2.2\n"
    );
    screenshots.push_back(ctx.captureScreenshot("block_layout_nested_list"));

    // Test 4: Blockquotes
    ctx.setContent(
        "# Blockquotes\n"
        "\n"
        "> This is a simple blockquote.\n"
        "> It spans multiple lines.\n"
        "\n"
        "Regular paragraph.\n"
        "\n"
        "> Nested blockquotes:\n"
        "> > This is nested inside.\n"
        "> Back to first level.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("block_layout_blockquote"));

    // Test 5: Horizontal rules
    ctx.setContent(
        "# Horizontal Rules\n"
        "\n"
        "Text before rule.\n"
        "\n"
        "---\n"
        "\n"
        "Text after first rule.\n"
        "\n"
        "***\n"
        "\n"
        "Text after second rule.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("block_layout_hr"));

    // Test 6: Mixed content
    ctx.setContent(
        "# Mixed Layout\n"
        "\n"
        "A paragraph with **bold** and *italic* text.\n"
        "\n"
        "> A blockquote with `inline code` inside.\n"
        "\n"
        "A list with formatting:\n"
        "\n"
        "1. **Bold item**\n"
        "2. *Italic item*\n"
        "3. Item with `code`\n"
        "\n"
        "---\n"
        "\n"
        "Final paragraph.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("block_layout_mixed"));

    // Test 7: Tables (if supported)
    ctx.setContent(
        "# Tables\n"
        "\n"
        "| Column 1 | Column 2 | Column 3 |\n"
        "|----------|----------|----------|\n"
        "| Cell 1   | Cell 2   | Cell 3   |\n"
        "| Cell 4   | Cell 5   | Cell 6   |\n"
        "\n"
        "Table with alignment:\n"
        "\n"
        "| Left | Center | Right |\n"
        "|:-----|:------:|------:|\n"
        "| L    |   C    |     R |\n"
    );
    screenshots.push_back(ctx.captureScreenshot("block_layout_tables"));

    // Test 8: Long content (scroll test)
    ctx.setContent(
        "# Long Content Test\n"
        "\n"
        "Line 1\n\nLine 2\n\nLine 3\n\nLine 4\n\nLine 5\n\n"
        "Line 6\n\nLine 7\n\nLine 8\n\nLine 9\n\nLine 10\n\n"
        "Line 11\n\nLine 12\n\nLine 13\n\nLine 14\n\nLine 15\n\n"
        "Line 16\n\nLine 17\n\nLine 18\n\nLine 19\n\nLine 20\n\n"
        "## End of Document\n"
    );
    screenshots.push_back(ctx.captureScreenshot("block_layout_scroll_top"));

    // Scroll down and capture again
    ctx.simulateScroll(-10);  // Scroll down
    screenshots.push_back(ctx.captureScreenshot("block_layout_scroll_middle", 1));

    TEST_PASS();
}

REGISTER_TEST(BlockLayout, testBlockLayout);
