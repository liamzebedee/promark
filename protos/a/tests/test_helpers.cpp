#include "test_helpers.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <random>
#include <sstream>
#include <iomanip>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../vendor/glfw-3.4/deps/stb_image_write.h"

// Generate a unique session ID (8 hex chars)
static std::string generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << dist(gen);
    return ss.str();
}

// TestContext implementation

TestContext::TestContext(int width, int height)
    : width(width)
    , height(height)
    , window(nullptr)
    , engine(nullptr)
    , framebuffer(0)
    , colorTexture(0)
    , depthRenderbuffer(0)
    , sessionId(generateSessionId()) {
    // Use environment variable for output directory, default to /tmp
    const char* envDir = std::getenv("TEST_OUTPUT_DIR");
    outputDir = envDir ? envDir : "/tmp/promark_tests";
}

TestContext::~TestContext() {
    if (framebuffer) {
        glDeleteFramebuffers(1, &framebuffer);
    }
    if (colorTexture) {
        glDeleteTextures(1, &colorTexture);
    }
    if (depthRenderbuffer) {
        glDeleteRenderbuffers(1, &depthRenderbuffer);
    }
    delete engine;
    if (window) {
        glfwDestroyWindow(window);
    }
}

bool TestContext::initialize() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    // Create invisible window for OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // Headless!

    window = glfwCreateWindow(width, height, "Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);

    // Setup framebuffer for offscreen rendering
    if (!setupFramebuffer()) {
        std::cerr << "Failed to setup framebuffer\n";
        return false;
    }

    // Create and initialize engine
    engine = new Engine();
    if (!engine->initialize()) {
        std::cerr << "Failed to initialize engine\n";
        return false;
    }

    // Create output directory
    mkdir(outputDir.c_str(), 0755);

    return true;
}

bool TestContext::setupFramebuffer() {
    // Generate framebuffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Create color texture
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    // Create depth renderbuffer
    glGenRenderbuffers(1, &depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);

    // Check framebuffer status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete: " << status << "\n";
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void TestContext::bindFramebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
}

void TestContext::unbindFramebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TestContext::setContent(const std::string& markdown) {
    if (engine) {
        engine->setContent(markdown);
    }
}

void TestContext::setCursorPosition(int pos) {
    // Navigate to position using keyboard:
    // 1. Go to document start (Ctrl+Up)
    // 2. Move right `pos` times
    simulateKey(GLFW_KEY_UP, GLFW_MOD_CONTROL);  // Go to start
    for (int i = 0; i < pos; i++) {
        simulateKey(GLFW_KEY_RIGHT, 0);  // Move right one position at a time
    }
}

void TestContext::setScrollOffset(float offset) {
    if (engine && offset != 0) {
        engine->handleScroll(0, -offset / 20.0);  // Approximate scroll units
    }
}

std::string TestContext::captureScreenshot(const std::string& testName, int screenshotIndex) {
    bindFramebuffer();

    // Render
    engine->render(width, height);

    // Read pixels
    std::vector<unsigned char> pixels(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    unbindFramebuffer();

    // Flip vertically (OpenGL has origin at bottom-left)
    std::vector<unsigned char> flipped(width * height * 4);
    for (int y = 0; y < height; y++) {
        memcpy(&flipped[y * width * 4],
               &pixels[(height - 1 - y) * width * 4],
               width * 4);
    }

    // Generate filename with session ID for parallel test support
    std::string filename = outputDir + "/" + testName + "_" + sessionId;
    if (screenshotIndex > 0) {
        filename += "_" + std::to_string(screenshotIndex);
    }
    filename += ".png";

    // Write PNG
    if (!stbi_write_png(filename.c_str(), width, height, 4, flipped.data(), width * 4)) {
        std::cerr << "Failed to write PNG: " << filename << "\n";
        return "";
    }

    return filename;
}

void TestContext::simulateKey(int key, int mods) {
    if (engine) {
        engine->handleKeyboard(key, 0, GLFW_PRESS, mods);
        engine->handleKeyboard(key, 0, GLFW_RELEASE, mods);
    }
}

void TestContext::simulateClick(double x, double y, int button) {
    if (engine) {
        engine->handleMouse(button, GLFW_PRESS, 0, x, y);
        engine->handleMouse(button, GLFW_RELEASE, 0, x, y);
    }
}

void TestContext::simulateMousePress(double x, double y, int button) {
    if (engine) {
        engine->handleMouse(button, GLFW_PRESS, 0, x, y);
    }
}

void TestContext::simulateMouseMove(double x, double y) {
    if (engine) {
        engine->handleMouseMove(x, y);
    }
}

void TestContext::simulateMouseRelease(double x, double y, int button) {
    if (engine) {
        engine->handleMouse(button, GLFW_RELEASE, 0, x, y);
    }
}

void TestContext::simulateScroll(double yOffset) {
    if (engine) {
        engine->handleScroll(0, yOffset);
    }
}

// TestRunner implementation

TestRunner& TestRunner::instance() {
    static TestRunner runner;
    return runner;
}

void TestRunner::registerTest(const std::string& name, TestFunction func) {
    tests.push_back({name, func});
}

int TestRunner::runAll() {
    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        TestContext ctx;
        if (!ctx.initialize()) {
            std::cerr << "FAIL: " << test.name << " - Failed to initialize test context\n";
            failed++;
            continue;
        }

        std::cout << "SESSION: " << ctx.getSessionId() << "\n";
        TestResult result = test.func(ctx);

        if (result.passed) {
            std::cout << "PASS: " << test.name << "\n";
            passed++;
        } else {
            std::cerr << "FAIL: " << test.name << " - " << result.errorMessage << "\n";
            failed++;
        }

        // Output screenshot paths
        for (const auto& path : result.screenshotPaths) {
            std::cout << "SCREENSHOT: " << path << "\n";
        }
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}

int TestRunner::runTest(const std::string& name) {
    for (const auto& test : tests) {
        if (test.name == name) {
            TestContext ctx;
            if (!ctx.initialize()) {
                std::cerr << "FAIL: " << test.name << " - Failed to initialize test context\n";
                return 1;
            }

            std::cout << "SESSION: " << ctx.getSessionId() << "\n";
            TestResult result = test.func(ctx);

            if (result.passed) {
                std::cout << "PASS: " << test.name << "\n";
            } else {
                std::cerr << "FAIL: " << test.name << " - " << result.errorMessage << "\n";
            }

            // Output screenshot paths
            for (const auto& path : result.screenshotPaths) {
                std::cout << "SCREENSHOT: " << path << "\n";
            }

            return result.passed ? 0 : 1;
        }
    }

    std::cerr << "Test not found: " << name << "\n";
    return 1;
}
