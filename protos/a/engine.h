#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <map>

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
    
    // FreeType font system
    FT_Library ft;
    FT_Face face;
    bool fontLoaded;
    
    struct Glyph {
        unsigned int textureID;
        int width;
        int height;
        int bearingX;
        int bearingY;
        int advance;
    };
    
    std::map<char, Glyph> glyphs;
    
    void renderText(const char* text, float x, float y);
    void renderChar(char c, float x, float y);
    bool initFreeType();
    bool loadFont(const char* fontPath);
    void loadGlyph(char c);
};