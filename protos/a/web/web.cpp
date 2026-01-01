// Web entry point for Emscripten build
#include "engine/engine.h"
#include <GLFW/glfw3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <string>

static Engine* engine = nullptr;
static GLFWwindow* window = nullptr;

// Callbacks
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (engine) {
        engine->handleKeyboard(key, scancode, action, mods);
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (engine) {
        engine->handleScroll(xoffset, yoffset);
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    if (engine) {
        engine->handleMouse(button, action, mods, x, y);
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y) {
    if (engine) {
        engine->handleMouseMove(x, y);

        // Update cursor style for links
        if (engine->isOverLink(x, y)) {
            glfwSetCursor(window, glfwCreateStandardCursor(GLFW_HAND_CURSOR));
        } else {
            glfwSetCursor(window, glfwCreateStandardCursor(GLFW_IBEAM_CURSOR));
        }
    }
}

void charCallback(GLFWwindow* window, unsigned int codepoint) {
    if (engine && codepoint >= 32 && codepoint < 127) {
        engine->handleKeyboard(codepoint, 0, GLFW_PRESS, 0);
    }
}

// Main loop called by Emscripten
void mainLoop() {
    if (!window || !engine) return;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    engine->render(width, height);
    glfwSwapBuffers(window);
    glfwPollEvents();
}

// Exported functions for JavaScript interaction
extern "C" {

EMSCRIPTEN_KEEPALIVE
void setContent(const char* content) {
    if (engine) {
        engine->setContent(std::string(content));
    }
}

EMSCRIPTEN_KEEPALIVE
const char* getContent() {
    static std::string content;
    if (engine) {
        content = engine->getContent();
        return content.c_str();
    }
    return "";
}

EMSCRIPTEN_KEEPALIVE
int isDirty() {
    return engine ? engine->isDirty() : 0;
}

EMSCRIPTEN_KEEPALIVE
void markClean() {
    if (engine) {
        engine->markClean();
    }
}

}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Request OpenGL ES 2.0 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);

    window = glfwCreateWindow(900, 700, "MD Editor", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Set up callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetCharCallback(window, charCallback);

    // Create and initialize engine
    engine = new Engine();
    if (!engine->initialize()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        delete engine;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Set default content
    engine->setContent("# Welcome to MD Editor\n\nThis is a **markdown** editor running in your browser!\n\n## Features\n\n- Real-time preview\n- Syntax highlighting\n- *Italic* and **bold** text\n\n```\nCode blocks work too!\n```\n\nStart typing to edit...\n");

    std::cout << "Web editor initialized" << std::endl;

    // Start main loop
    emscripten_set_main_loop(mainLoop, 0, 1);

    // Cleanup (never reached in Emscripten)
    delete engine;
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
