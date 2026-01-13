#include "test_helpers.h"

// Test inline code and code blocks
TestResult testInlineBlock(TestContext& ctx) {
    std::vector<std::string> screenshots;

    // Test 1: Inline code
    ctx.setContent(
        "# Inline Code\n"
        "\n"
        "Use the `printf()` function to print output.\n"
        "\n"
        "Multiple `code` spans `in one` line.\n"
        "\n"
        "Code with **bold `code` inside** formatting.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("inline_block_inline_code"));

    // Test 2: Fenced code blocks
    ctx.setContent(
        "# Code Blocks\n"
        "\n"
        "```cpp\n"
        "#include <iostream>\n"
        "\n"
        "int main() {\n"
        "    std::cout << \"Hello World\" << std::endl;\n"
        "    return 0;\n"
        "}\n"
        "```\n"
        "\n"
        "Text after code block.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("inline_block_fenced_code"));

    // Test 3: Code blocks without language
    ctx.setContent(
        "# Plain Code Block\n"
        "\n"
        "```\n"
        "This is a plain code block\n"
        "with multiple lines\n"
        "and no syntax highlighting\n"
        "```\n"
        "\n"
        "More text below.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("inline_block_plain_code"));

    // Test 4: Multiple code blocks
    ctx.setContent(
        "# Multiple Blocks\n"
        "\n"
        "First block:\n"
        "\n"
        "```python\n"
        "def hello():\n"
        "    print(\"Hello\")\n"
        "```\n"
        "\n"
        "Second block:\n"
        "\n"
        "```javascript\n"
        "function hello() {\n"
        "    console.log(\"Hello\");\n"
        "}\n"
        "```\n"
    );
    screenshots.push_back(ctx.captureScreenshot("inline_block_multiple_blocks"));

    // Test 5: Indented code (4 spaces)
    ctx.setContent(
        "# Indented Code\n"
        "\n"
        "Here is some indented code:\n"
        "\n"
        "    function test() {\n"
        "        return true;\n"
        "    }\n"
        "\n"
        "Back to normal text.\n"
    );
    screenshots.push_back(ctx.captureScreenshot("inline_block_indented_code"));

    TEST_PASS();
}

REGISTER_TEST(InlineBlock, testInlineBlock);
