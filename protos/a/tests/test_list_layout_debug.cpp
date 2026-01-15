#include "test_helpers.h"
#include <iostream>

// Debug test to understand list layout positioning
TestResult testListLayoutDebug(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Simple list followed by another list
    engine->setContent(
        "- First item\n"
        "- Second item\n"
        "  - Nested item\n"
        "  - Another nested\n"
        "- Third item\n"
        "\n"
        "1. Numbered one\n"
        "2. Numbered two\n"
        "3. Numbered three\n"
        "\n"
        "## Table\n"
        "\n"
        "| Name | Value |\n"
        "|------|-------|\n"
        "| foo  | 123   |"
    );

    // Capture screenshot to see overlap
    screenshots.push_back(ctx.captureScreenshot("list_layout_debug"));
    printf("Screenshot: %s\n", screenshots.back().c_str());

    TEST_PASS();
}

// Test list structure without tables
TestResult testListOnlyLayout(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    engine->setContent(
        "# Lists\n"
        "\n"
        "Paragraph before list.\n"
        "\n"
        "- First item\n"
        "- Second item\n"
        "  - Nested A\n"
        "  - Nested B\n"
        "- Third item\n"
        "\n"
        "Paragraph after first list.\n"
        "\n"
        "1. One\n"
        "2. Two\n"
        "3. Three\n"
        "\n"
        "Final paragraph."
    );

    screenshots.push_back(ctx.captureScreenshot("list_only_debug"));
    printf("Screenshot: %s\n", screenshots.back().c_str());

    TEST_PASS();
}

// Test to check if lists work fine without nested items
TestResult testFlatListLayout(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    engine->setContent(
        "# Test\n"
        "\n"
        "- Item one\n"
        "- Item two\n"
        "- Item three\n"
        "\n"
        "Paragraph.\n"
        "\n"
        "1. First\n"
        "2. Second\n"
        "3. Third"
    );

    screenshots.push_back(ctx.captureScreenshot("flat_list_debug"));
    printf("Screenshot: %s\n", screenshots.back().c_str());

    TEST_PASS();
}

REGISTER_TEST(ListLayoutDebug, testListLayoutDebug);
REGISTER_TEST(ListOnlyLayout, testListOnlyLayout);
REGISTER_TEST(FlatListLayout, testFlatListLayout);
