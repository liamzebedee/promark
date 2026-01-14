#include "test_helpers.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>

// Helper function to read file contents
static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Test that Down arrow can navigate through the entire document without getting stuck
TestResult test_down_arrow_full_document(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Load the long document (tests run from build directory, so go up one level)
    std::string content = readFile("../resources/long-post.md");
    if (content.empty()) {
        return TestResult{"test_down_arrow_full_document", false,
            "Could not load ../resources/long-post.md", screenshots};
    }

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Start at position 0
    ctx.simulateKey(GLFW_KEY_UP, GLFW_MOD_CONTROL);  // Go to document start
    engine->render(ctx.getWidth(), ctx.getHeight());

    int lastPos = engine->getCursorPosition();
    int documentLength = static_cast<int>(content.length());
    int stuckCount = 0;
    int maxIterations = 1000;  // Safety limit
    std::set<int> visitedPositions;

    std::cout << "  Document length: " << documentLength << std::endl;
    std::cout << "  Starting at position: " << lastPos << std::endl;

    // Navigate down through the document
    for (int i = 0; i < maxIterations; i++) {
        ctx.simulateKey(GLFW_KEY_DOWN);
        engine->render(ctx.getWidth(), ctx.getHeight());

        int newPos = engine->getCursorPosition();

        // Check if cursor moved
        if (newPos == lastPos) {
            stuckCount++;
            // If stuck 3 times in a row, we have a problem
            if (stuckCount >= 3) {
                // Unless we're at the end of the document
                if (newPos < documentLength - 100) {  // Allow slack for last visual line at end
                    std::cout << "  Stuck at position " << newPos << " after " << i << " Down presses" << std::endl;
                    screenshots.push_back(ctx.captureScreenshot("down_arrow_stuck", 0));
                    return TestResult{"test_down_arrow_full_document", false,
                        "Cursor got stuck at position " + std::to_string(newPos) +
                        " (document length: " + std::to_string(documentLength) + ")",
                        screenshots};
                } else {
                    // We reached the end successfully
                    std::cout << "  Reached end of document at position " << newPos << std::endl;
                    break;
                }
            }
        } else {
            stuckCount = 0;
            visitedPositions.insert(newPos);
        }

        lastPos = newPos;

        // Check for unexpected position
        if (newPos < 0 || newPos > documentLength) {
            return TestResult{"test_down_arrow_full_document", false,
                "Cursor went out of bounds: " + std::to_string(newPos), screenshots};
        }
    }

    std::cout << "  Final position: " << lastPos << std::endl;
    std::cout << "  Unique positions visited: " << visitedPositions.size() << std::endl;

    TEST_PASS();
}

// Test that Up arrow can navigate back to the start without getting stuck
TestResult test_up_arrow_full_document(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Load the long document
    std::string content = readFile("../resources/long-post.md");
    if (content.empty()) {
        return TestResult{"test_up_arrow_full_document", false,
            "Could not load ../resources/long-post.md", screenshots};
    }

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Start at the end of the document
    ctx.simulateKey(GLFW_KEY_DOWN, GLFW_MOD_CONTROL);  // Go to document end
    engine->render(ctx.getWidth(), ctx.getHeight());

    int lastPos = engine->getCursorPosition();
    int stuckCount = 0;
    int maxIterations = 1000;

    std::cout << "  Starting at position: " << lastPos << std::endl;

    // Navigate up through the document
    for (int i = 0; i < maxIterations; i++) {
        ctx.simulateKey(GLFW_KEY_UP);
        engine->render(ctx.getWidth(), ctx.getHeight());

        int newPos = engine->getCursorPosition();

        if (newPos == lastPos) {
            stuckCount++;
            if (stuckCount >= 3) {
                if (newPos > 10) {  // Allow some slack for start of document
                    std::cout << "  Stuck at position " << newPos << " after " << i << " Up presses" << std::endl;
                    screenshots.push_back(ctx.captureScreenshot("up_arrow_stuck", 0));
                    return TestResult{"test_up_arrow_full_document", false,
                        "Cursor got stuck at position " + std::to_string(newPos),
                        screenshots};
                } else {
                    std::cout << "  Reached start of document at position " << newPos << std::endl;
                    break;
                }
            }
        } else {
            stuckCount = 0;
        }

        lastPos = newPos;
    }

    std::cout << "  Final position: " << lastPos << std::endl;

    TEST_PASS();
}

// Test that Right arrow can navigate through every character
TestResult test_right_arrow_full_document(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Load the long document
    std::string content = readFile("../resources/long-post.md");
    if (content.empty()) {
        return TestResult{"test_right_arrow_full_document", false,
            "Could not load ../resources/long-post.md", screenshots};
    }

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Start at position 0
    ctx.simulateKey(GLFW_KEY_UP, GLFW_MOD_CONTROL);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int lastPos = engine->getCursorPosition();
    int documentLength = static_cast<int>(content.length());
    int stuckCount = 0;

    std::cout << "  Document length: " << documentLength << std::endl;
    std::cout << "  Starting at position: " << lastPos << std::endl;

    // Navigate right through every character
    for (int i = 0; i < documentLength + 10; i++) {
        ctx.simulateKey(GLFW_KEY_RIGHT);
        // Don't render every time - too slow
        if (i % 100 == 0) {
            engine->render(ctx.getWidth(), ctx.getHeight());
        }

        int newPos = engine->getCursorPosition();

        if (newPos == lastPos) {
            stuckCount++;
            if (stuckCount >= 3) {
                if (newPos < documentLength) {
                    std::cout << "  Stuck at position " << newPos << " after " << i << " Right presses" << std::endl;
                    return TestResult{"test_right_arrow_full_document", false,
                        "Cursor got stuck at position " + std::to_string(newPos) +
                        " (document length: " + std::to_string(documentLength) + ")",
                        screenshots};
                } else {
                    std::cout << "  Reached end at position " << newPos << std::endl;
                    break;
                }
            }
        } else {
            stuckCount = 0;
        }

        lastPos = newPos;
    }

    std::cout << "  Final position: " << lastPos << std::endl;

    // Verify we reached the end
    if (lastPos != documentLength) {
        return TestResult{"test_right_arrow_full_document", false,
            "Did not reach end of document. Expected: " + std::to_string(documentLength) +
            ", got: " + std::to_string(lastPos), screenshots};
    }

    TEST_PASS();
}

// Test that Left arrow can navigate back to start
TestResult test_left_arrow_full_document(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Load the long document
    std::string content = readFile("../resources/long-post.md");
    if (content.empty()) {
        return TestResult{"test_left_arrow_full_document", false,
            "Could not load ../resources/long-post.md", screenshots};
    }

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());

    // Start at the end
    ctx.simulateKey(GLFW_KEY_DOWN, GLFW_MOD_CONTROL);
    engine->render(ctx.getWidth(), ctx.getHeight());

    int lastPos = engine->getCursorPosition();
    int stuckCount = 0;
    int documentLength = static_cast<int>(content.length());

    std::cout << "  Starting at position: " << lastPos << std::endl;

    // Navigate left through every character
    for (int i = 0; i < documentLength + 10; i++) {
        ctx.simulateKey(GLFW_KEY_LEFT);
        if (i % 100 == 0) {
            engine->render(ctx.getWidth(), ctx.getHeight());
        }

        int newPos = engine->getCursorPosition();

        if (newPos == lastPos) {
            stuckCount++;
            if (stuckCount >= 3) {
                if (newPos > 0) {
                    std::cout << "  Stuck at position " << newPos << " after " << i << " Left presses" << std::endl;
                    return TestResult{"test_left_arrow_full_document", false,
                        "Cursor got stuck at position " + std::to_string(newPos),
                        screenshots};
                } else {
                    std::cout << "  Reached start at position " << newPos << std::endl;
                    break;
                }
            }
        } else {
            stuckCount = 0;
        }

        lastPos = newPos;
    }

    std::cout << "  Final position: " << lastPos << std::endl;

    // Verify we reached the start
    if (lastPos != 0) {
        return TestResult{"test_left_arrow_full_document", false,
            "Did not reach start of document. Expected: 0, got: " + std::to_string(lastPos),
            screenshots};
    }

    TEST_PASS();
}

REGISTER_TEST(down_arrow_full_document, test_down_arrow_full_document);
REGISTER_TEST(up_arrow_full_document, test_up_arrow_full_document);
REGISTER_TEST(right_arrow_full_document, test_right_arrow_full_document);
REGISTER_TEST(left_arrow_full_document, test_left_arrow_full_document);
