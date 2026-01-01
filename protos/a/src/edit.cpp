#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "engine/engine.h"

Engine* engine = nullptr;
GLFWwindow* window = nullptr;
std::string filePath;
std::string fileName;
std::string diskContent;  // Content as it exists on disk
bool lastDirtyState = false;

bool isDirty() {
    if (!engine) return false;
    return engine->getContent() != diskContent;
}

void updateWindowTitle() {
    bool currentDirty = isDirty();
    if (currentDirty != lastDirtyState) {
        std::string title = fileName;
        if (currentDirty) {
            title += " [unsaved]";
        }
        glfwSetWindowTitle(window, title.c_str());
        lastDirtyState = currentDirty;
    }
}

bool saveFile() {
    if (!engine || filePath.empty()) return false;

    std::string content = engine->getContent();

    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to save file: " << filePath << std::endl;
        return false;
    }

    file << content;
    file.close();

    diskContent = content;  // Update disk content reference
    updateWindowTitle();
    return true;
}

std::string loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Handle Cmd+S / Ctrl+S for save
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        bool cmdOrCtrl = (mods & GLFW_MOD_SUPER) || (mods & GLFW_MOD_CONTROL);
        if (cmdOrCtrl && key == GLFW_KEY_S) {
            saveFile();
            return;
        }
    }

    if (engine) {
        engine->handleKeyboard(key, scancode, action, mods);
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (engine) {
        engine->handleScroll(xoffset, yoffset);
    }
}

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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: mdedit <file.md>" << std::endl;
        return 1;
    }

    filePath = argv[1];

    // Extract filename from path
    size_t lastSlash = filePath.find_last_of("/\\");
    fileName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

    window = glfwCreateWindow(800, 600, fileName.c_str(), nullptr, nullptr);
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

    GLFWcursor* ibeamCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    glfwSetCursor(window, ibeamCursor);

    engine = new Engine();
    if (!engine->initialize()) {
        std::cerr << "Failed to initialize engine\n";
        delete engine;
        glfwTerminate();
        return -1;
    }

    // Load file content
    std::string content = loadFile(filePath);
    diskContent = content;  // Store what's on disk
    engine->setContent(content);

    updateWindowTitle();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        engine->render(width, height);

        updateWindowTitle();

        glfwSwapBuffers(window);
    }

    delete engine;
    glfwDestroyCursor(ibeamCursor);
    glfwTerminate();
    return 0;
}
