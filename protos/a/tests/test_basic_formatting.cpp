#include "test_helpers.h"

// Test basic text formatting: bold, italic, headers
TestResult testBasicFormatting(TestContext& ctx) {
    std::vector<std::string> screenshots;

    // Test 1: Headers (H1-H6)
    ctx.setContent(
        "# Heading 1\n"
        "## Heading 2\n"
        "### Heading 3\n"
        "#### Heading 4\n"
        "##### Heading 5\n"
        "###### Heading 6\n"
        "\n"
        "Normal paragraph text.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("basic_formatting_headers"));

    // Test 2: Bold and Italic
    ctx.setContent(
        "# Text Styles\n"
        "\n"
        "This is **bold text** in a paragraph.\n"
        "\n"
        "This is *italic text* in a paragraph.\n"
        "\n"
        "This is ***bold and italic*** combined.\n"
        "\n"
        "Mix of **bold** and *italic* and normal.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("basic_formatting_bold_italic"));

    // Test 3: Links
    ctx.setContent(
        "# Links\n"
        "\n"
        "Here is a [link to example](https://example.com) in text.\n"
        "\n"
        "Multiple [first link](https://first.com) and [second link](https://second.com) links.\n"
        "\n"
        "**Bold [link](https://bold.com)** inside bold text.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("basic_formatting_links"));

    // Test 4: Strikethrough
    ctx.setContent(
        "# Strikethrough\n"
        "\n"
        "This is ~~deleted text~~ with strikethrough.\n"
        "\n"
        "Combine ~~strikethrough with **bold**~~ text.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("basic_formatting_strikethrough"));

    TEST_PASS();
}

REGISTER_TEST(BasicFormatting, testBasicFormatting);
