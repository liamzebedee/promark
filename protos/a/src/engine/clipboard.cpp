#include "clipboard.h"
#include <GLFW/glfw3.h>

// Static member initialization
Clipboard::GetTextFn Clipboard::s_getText = nullptr;
Clipboard::SetTextFn Clipboard::s_setText = nullptr;
bool Clipboard::s_useCustom = false;

void Clipboard::setHandlers(GetTextFn getText, SetTextFn setText) {
    s_getText = getText;
    s_setText = setText;
    s_useCustom = (getText != nullptr && setText != nullptr);
}

void Clipboard::useDefaultHandlers() {
    s_getText = nullptr;
    s_setText = nullptr;
    s_useCustom = false;
}

std::string Clipboard::getText() {
    if (s_useCustom && s_getText) {
        return s_getText();
    }

    // Default: use GLFW clipboard
    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        const char* text = glfwGetClipboardString(window);
        return text ? std::string(text) : "";
    }
    return "";
}

void Clipboard::setText(const std::string& text) {
    if (s_useCustom && s_setText) {
        s_setText(text);
        return;
    }

    // Default: use GLFW clipboard
    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        glfwSetClipboardString(window, text.c_str());
    }
}

bool Clipboard::hasCustomHandlers() {
    return s_useCustom;
}
