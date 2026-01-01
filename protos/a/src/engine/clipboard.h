#pragma once
#include <string>
#include <functional>

// Platform-agnostic clipboard interface
// Desktop uses GLFW, web can provide custom implementations via callbacks
class Clipboard {
public:
    using GetTextFn = std::function<std::string()>;
    using SetTextFn = std::function<void(const std::string&)>;

    // Set custom clipboard handlers (for web platform)
    static void setHandlers(GetTextFn getText, SetTextFn setText);

    // Use default GLFW clipboard (for desktop)
    static void useDefaultHandlers();

    // Clipboard operations
    static std::string getText();
    static void setText(const std::string& text);

    // Check if using custom handlers
    static bool hasCustomHandlers();

private:
    static GetTextFn s_getText;
    static SetTextFn s_setText;
    static bool s_useCustom;
};
