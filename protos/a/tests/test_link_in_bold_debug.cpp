#include "test_helpers.h"
#include "../src/engine/markdown_parser.h"
#include <iostream>

// Recursively print the tree structure
void printTree(MarkdownObject* obj, int depth = 0) {
    std::string indent(depth * 2, ' ');
    std::cout << indent << "Type: " << static_cast<int>(obj->getType());

    if (!obj->getText().empty()) {
        std::cout << " Text: \"" << obj->getText() << "\"";
    }
    if (obj->getType() == MarkdownObjectType::Link) {
        auto link = static_cast<LinkObject*>(obj);
        std::cout << " URL: \"" << link->getUrl() << "\"";
    }
    std::cout << std::endl;

    for (const auto& child : obj->getChildren()) {
        printTree(child.get(), depth + 1);
    }
}

TestResult testLinkInBoldParsing(TestContext& ctx) {
    std::vector<std::string> screenshots;
    MarkdownParser parser;

    // Test the exact problematic input
    std::string input = "**Bold [link](https://bold.com)** inside bold text.\n";

    std::cout << "\n=== Parsing: '" << input << "' ===" << std::endl;

    auto doc = parser.parse(input);

    std::cout << "\n=== Parse Tree ===" << std::endl;
    printTree(doc.get());

    // Check structure - should be:
    // Document
    //   Paragraph
    //     Strong
    //       Text("Bold ")
    //       Link(https://bold.com)
    //         Text("link")
    //     Text(" inside bold text.")

    TEST_ASSERT(doc->getChildren().size() >= 1, "Should have at least 1 paragraph");

    auto& para = doc->getChildren()[0];
    std::cout << "\nParagraph has " << para->getChildren().size() << " children" << std::endl;

    for (size_t i = 0; i < para->getChildren().size(); ++i) {
        auto& child = para->getChildren()[i];
        std::cout << "Child " << i << ": type=" << static_cast<int>(child->getType());
        if (!child->getText().empty()) {
            std::cout << " text=\"" << child->getText() << "\"";
        }
        std::cout << " children=" << child->getChildren().size() << std::endl;

        // If it's a Strong node, print its children
        if (child->getType() == MarkdownObjectType::Strong) {
            for (size_t j = 0; j < child->getChildren().size(); ++j) {
                auto& strongChild = child->getChildren()[j];
                std::cout << "  Strong child " << j << ": type=" << static_cast<int>(strongChild->getType());
                if (strongChild->getType() == MarkdownObjectType::Link) {
                    auto* link = static_cast<LinkObject*>(strongChild.get());
                    std::cout << " url=\"" << link->getUrl() << "\"";
                }
                if (!strongChild->getText().empty()) {
                    std::cout << " text=\"" << strongChild->getText() << "\"";
                }
                std::cout << std::endl;
            }
        }
    }

    TEST_PASS();
}

// Also test simpler case
TestResult testSimpleLinkInBold(TestContext& ctx) {
    std::vector<std::string> screenshots;
    MarkdownParser parser;

    std::string input = "**[link](url)**\n";

    std::cout << "\n=== Parsing: '" << input << "' ===" << std::endl;

    auto doc = parser.parse(input);

    std::cout << "\n=== Parse Tree ===" << std::endl;
    printTree(doc.get());

    TEST_PASS();
}

// Visual test
TestResult testLinkInBoldVisual(TestContext& ctx) {
    std::vector<std::string> screenshots;

    ctx.setContent("**Bold [link](https://bold.com)** inside bold text.");
    screenshots.push_back(ctx.captureScreenshot("link_in_bold_visual"));

    // Try simpler case
    ctx.setContent("**[link](https://example.com)**");
    screenshots.push_back(ctx.captureScreenshot("link_in_bold_simple"));

    // Test link first, then bold
    ctx.setContent("[link **with bold**](https://example.com)");
    screenshots.push_back(ctx.captureScreenshot("bold_in_link_visual"));

    TEST_PASS();
}

REGISTER_TEST(LinkInBoldParsing, testLinkInBoldParsing);
REGISTER_TEST(SimpleLinkInBold, testSimpleLinkInBold);
REGISTER_TEST(LinkInBoldVisual, testLinkInBoldVisual);
