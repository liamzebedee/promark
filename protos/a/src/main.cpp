#include <GLFW/glfw3.h>
#include <iostream>
#include "engine/engine.h"

Engine* engine = nullptr;

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

// Get scale factor for Retina displays
void getDisplayScale(GLFWwindow* window, float& scaleX, float& scaleY) {
    int winW, winH, fbW, fbH;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);
    scaleX = (winW > 0) ? (float)fbW / winW : 1.0f;
    scaleY = (winH > 0) ? (float)fbH / winH : 1.0f;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (engine) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        float scaleX, scaleY;
        getDisplayScale(window, scaleX, scaleY);
        engine->handleMouse(button, action, mods, xpos * scaleX, ypos * scaleY);
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (engine) {
        float scaleX, scaleY;
        getDisplayScale(window, scaleX, scaleY);
        engine->handleMouseMove(xpos * scaleX, ypos * scaleY);
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "MD Editor", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    // Set I-beam cursor for text editing
    GLFWcursor* ibeamCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    glfwSetCursor(window, ibeamCursor);

    engine = new Engine();
    if (!engine->initialize()) {
        std::cerr << "Failed to initialize engine\n";
        delete engine;
        glfwTerminate();
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        engine->render(width, height);

        glfwSwapBuffers(window);
    }

    delete engine;
    glfwDestroyCursor(ibeamCursor);
    glfwTerminate();
    return 0;
}