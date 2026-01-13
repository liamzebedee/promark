#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>
#include "engine/engine.h"

Engine* engine = nullptr;
GLFWwindow* window = nullptr;
std::string filePath;
std::string fileName;
bool lastDirtyState = false;

bool isDirty() {
    if (!engine) return false;
    return engine->isDirty();
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

    engine->markClean();  // Mark buffer as saved
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

// Image drag-and-drop support
bool isImageFile(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot + 1);
    // Convert to lowercase
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

std::string getDirectoryPath(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash == std::string::npos) return ".";
    return filePath.substr(0, lastSlash);
}

std::string computeRelativePath(const std::string& fromFile, const std::string& toFile) {
    // Get directory of the markdown file
    std::string fromDir = getDirectoryPath(fromFile);

    // If image is in the same directory, just use filename
    std::string imageDir = getDirectoryPath(toFile);
    std::string imageName = toFile.substr(toFile.find_last_of("/\\") + 1);

    if (fromDir == imageDir) {
        return imageName;
    }

    // Check if image is in a subdirectory of the document
    if (toFile.find(fromDir) == 0) {
        return toFile.substr(fromDir.length() + 1);
    }

    // Fallback to absolute path
    return toFile;
}

std::string buildImageMarkdown(const std::string& imagePath) {
    // Extract filename for alt text
    size_t lastSlash = imagePath.find_last_of("/\\");
    std::string altText = (lastSlash != std::string::npos)
        ? imagePath.substr(lastSlash + 1)
        : imagePath;
    // Remove extension from alt text
    size_t dot = altText.find_last_of('.');
    if (dot != std::string::npos) {
        altText = altText.substr(0, dot);
    }

    // Try to compute a relative path if document is saved
    std::string pathToUse;
    bool useBase64 = false;

    if (!filePath.empty()) {
        std::string relativePath = computeRelativePath(filePath, imagePath);
        // If we got an absolute path back (starts with /), fall back to base64
        if (!relativePath.empty() && relativePath[0] != '/') {
            pathToUse = relativePath;
        } else {
            useBase64 = true;
        }
    } else {
        useBase64 = true;
    }

    if (useBase64) {
        // Use base64 data URI
        std::string mimeType = getMimeType(imagePath);
        std::string base64 = encodeFileToBase64(imagePath);
        if (base64.empty()) return "";
        return "\n![" + altText + "](data:" + mimeType + ";base64," + base64 + ")\n";
    } else {
        return "\n![" + altText + "](" + pathToUse + ")\n";
    }
}

void dropCallback(GLFWwindow* win, int count, const char** paths) {
    if (!engine) return;

    for (int i = 0; i < count; i++) {
        std::string path = paths[i];
        if (isImageFile(path)) {
            std::string markdown = buildImageMarkdown(path);
            if (!markdown.empty()) {
                engine->insertText(markdown);
            }
        }
    }
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

GLFWcursor* ibeamCursor = nullptr;
GLFWcursor* handCursor = nullptr;
bool currentlyOverLink = false;

void cursorPosCallback(GLFWwindow* win, double xpos, double ypos) {
    if (engine) {
        float scaleX, scaleY;
        getDisplayScale(win, scaleX, scaleY);
        double scaledX = xpos * scaleX;
        double scaledY = ypos * scaleY;
        engine->handleMouseMove(scaledX, scaledY);

        // Change cursor when over link
        bool overLink = engine->isOverLink(scaledX, scaledY);
        if (overLink != currentlyOverLink) {
            currentlyOverLink = overLink;
            glfwSetCursor(win, overLink ? handCursor : ibeamCursor);
        }
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

    // Hide window until first frame is rendered
    glfwHideWindow(window);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetDropCallback(window, dropCallback);

    ibeamCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    handCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
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
    engine->setContent(content);  // setContent() already marks buffer as clean

    updateWindowTitle();

    bool windowShown = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        engine->render(width, height);

        updateWindowTitle();

        glfwSwapBuffers(window);

        // Show window after first frame is fully rendered
        if (!windowShown) {
            glfwShowWindow(window);
            windowShown = true;
        }
    }

    delete engine;
    glfwDestroyCursor(ibeamCursor);
    glfwDestroyCursor(handCursor);
    glfwTerminate();
    return 0;
}
