#pragma once

// Include GL headers before GLFW to ensure extension prototypes are available
#include "../src/engine/gl_includes.h"
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <functional>
#include "../src/engine/engine.h"

// Test context for offscreen rendering
class TestContext {
public:
    TestContext(int width = 800, int height = 600);
    ~TestContext();

    bool initialize();

    // Set markdown content and cursor/scroll position
    void setContent(const std::string& markdown);
    void setCursorPosition(int pos);
    void setScrollOffset(float offset);

    // Render and capture to PNG
    // Returns the path to the saved screenshot
    std::string captureScreenshot(const std::string& testName, int screenshotIndex = 0);

    // Simulate input
    void simulateKey(int key, int mods = 0);
    void simulateClick(double x, double y, int button = GLFW_MOUSE_BUTTON_LEFT);
    void simulateScroll(double yOffset);

    // Access engine for advanced testing
    Engine* getEngine() { return engine; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    int width;
    int height;
    GLFWwindow* window;
    Engine* engine;
    GLuint framebuffer;
    GLuint colorTexture;
    GLuint depthRenderbuffer;
    std::string outputDir;

    bool setupFramebuffer();
    void bindFramebuffer();
    void unbindFramebuffer();
};

// Test result
struct TestResult {
    std::string testName;
    bool passed;
    std::string errorMessage;
    std::vector<std::string> screenshotPaths;
};

// Test function signature
using TestFunction = std::function<TestResult(TestContext&)>;

// Test registration
struct TestCase {
    std::string name;
    TestFunction func;
};

// Test runner - runs all registered tests and outputs results
class TestRunner {
public:
    static TestRunner& instance();

    void registerTest(const std::string& name, TestFunction func);
    int runAll();
    int runTest(const std::string& name);

private:
    std::vector<TestCase> tests;
};

// Macro for easy test registration
#define REGISTER_TEST(name, func) \
    static bool _test_##name##_registered = []() { \
        TestRunner::instance().registerTest(#name, func); \
        return true; \
    }()

// Helper macros for test assertions
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            return TestResult{__func__, false, message, {}}; \
        } \
    } while(0)

#define TEST_PASS() \
    return TestResult{__func__, true, "", screenshots}
