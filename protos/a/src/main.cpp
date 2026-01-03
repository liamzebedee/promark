#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cctype>
#include "engine/engine.h"

Engine* engine = nullptr;

// Image drag-and-drop support (always base64 since no save path)
bool isImageFile(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot + 1);
    for (char& c : ext) c = std::tolower(c);
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "webp" || ext == "bmp";
}

std::string getMimeType(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "image/png";
    std::string ext = path.substr(dot + 1);
    for (char& c : ext) c = std::tolower(c);
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "bmp") return "image/bmp";
    return "image/png";
}

std::string encodeFileToBase64(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

    static const char* base64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t n = data[i] << 16;
        if (i + 1 < data.size()) n |= data[i + 1] << 8;
        if (i + 2 < data.size()) n |= data[i + 2];

        result += base64Chars[(n >> 18) & 0x3F];
        result += base64Chars[(n >> 12) & 0x3F];
        result += (i + 1 < data.size()) ? base64Chars[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < data.size()) ? base64Chars[n & 0x3F] : '=';
    }

    return result;
}

void dropCallback(GLFWwindow* win, int count, const char** paths) {
    if (!engine) return;

    for (int i = 0; i < count; i++) {
        std::string path = paths[i];
        if (isImageFile(path)) {
            // Extract filename for alt text
            size_t lastSlash = path.find_last_of("/\\");
            std::string altText = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
            size_t dot = altText.find_last_of('.');
            if (dot != std::string::npos) altText = altText.substr(0, dot);

            std::string mimeType = getMimeType(path);
            std::string base64 = encodeFileToBase64(path);
            if (!base64.empty()) {
                std::string markdown = "\n![" + altText + "](data:" + mimeType + ";base64," + base64 + ")\n";
                engine->insertText(markdown);
            }
        }
    }
}

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

    // Hide window until first frame is rendered
    glfwHideWindow(window);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetDropCallback(window, dropCallback);

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

    engine->setContent("# Lists Demo\n\nBullet list:\n\n- Apple\n- Banana\n- Cherry\n\nNumbered list:\n\n1. First item\n2. Second item\n3. Third item\n");

    bool windowShown = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Check if engine wants to close
        if (engine->shouldClose()) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        engine->render(width, height);

        glfwSwapBuffers(window);

        // Show window after first frame is fully rendered
        if (!windowShown) {
            glfwShowWindow(window);
            windowShown = true;
        }
    }

    delete engine;
    glfwDestroyCursor(ibeamCursor);
    glfwTerminate();
    return 0;
}