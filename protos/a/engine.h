#pragma once

class Engine {
public:
    Engine();
    ~Engine();
    
    bool initialize();
    void render(int width, int height);
    void handleKeyboard(int key, int scancode, int action, int mods);
    void handleScroll(double xoffset, double yoffset);
    void handleMouse(int button, int action, int mods, double x, double y);

private:
    float scrollOffset;
    char inputBuffer[1024];
    int inputLength;
    
    void renderText(const char* text, float x, float y);
    void renderChar(char c, float x, float y);
    void initFont();
    
    unsigned char fontData[128][8];
};