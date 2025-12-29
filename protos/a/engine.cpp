#include "engine.h"
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>

Engine::Engine() : scrollOffset(0.0f), inputLength(0), fontLoaded(false) {
    memset(inputBuffer, 0, sizeof(inputBuffer));
}

Engine::~Engine() {
    if (fontLoaded) {
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }
    
    // Clean up OpenGL textures
    for (auto& glyph : glyphs) {
        glDeleteTextures(1, &glyph.second.textureID);
    }
}

bool Engine::initialize() {
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    
    // Initialize FreeType
    if (!initFreeType()) {
        std::cerr << "Failed to initialize FreeType" << std::endl;
        return false;
    }
    
    // Try to load system font (Helvetica or Arial)
    const char* fontPaths[] = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Arial.ttf", 
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Times.ttc" // fallback
    };
    
    bool fontLoadedSuccessfully = false;
    for (const char* fontPath : fontPaths) {
        if (loadFont(fontPath)) {
            std::cout << "Loaded font: " << fontPath << std::endl;
            fontLoadedSuccessfully = true;
            break;
        }
    }
    
    if (!fontLoadedSuccessfully) {
        std::cerr << "Failed to load any system font" << std::endl;
        return false;
    }
    
    // Test basic OpenGL functionality
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error during initialization: " << error << std::endl;
        return false;
    }
    
    std::cout << "Engine initialized successfully" << std::endl;
    return true;
}

void Engine::render(int width, int height) {
    static int frameCount = 0;
    frameCount++;
    
    if (frameCount % 60 == 0) { // Log every 60 frames
        std::cout << "Render frame " << frameCount << ", size: " << width << "x" << height << std::endl;
        std::cout << "Input buffer: '" << inputBuffer << "'" << std::endl;
    }
    
    glViewport(0, 0, width, height);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // White background
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Test rendering - large colored rectangles across full screen
    std::cout << "Drawing test rectangles..." << std::endl;
    
    glColor3f(1.0f, 0.0f, 0.0f); // Red
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(width/2, 0);
    glVertex2f(width/2, height/2);
    glVertex2f(0, height/2);
    glEnd();
    
    glColor3f(0.0f, 1.0f, 0.0f); // Green  
    glBegin(GL_QUADS);
    glVertex2f(width/2, 0);
    glVertex2f(width, 0);
    glVertex2f(width, height/2);
    glVertex2f(width/2, height/2);
    glEnd();
    
    glColor3f(0.0f, 0.0f, 1.0f); // Blue
    glBegin(GL_QUADS);
    glVertex2f(0, height/2);
    glVertex2f(width/2, height/2);
    glVertex2f(width/2, height);
    glVertex2f(0, height);
    glEnd();
    
    glColor3f(1.0f, 1.0f, 0.0f); // Yellow
    glBegin(GL_QUADS);
    glVertex2f(width/2, height/2);
    glVertex2f(width, height/2);
    glVertex2f(width, height);
    glVertex2f(width/2, height);
    glEnd();
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "OpenGL error during rendering: " << error << std::endl;
    }

    char statusText[256];
    snprintf(statusText, sizeof(statusText), "Scroll: %.2f", scrollOffset);
    renderText(statusText, 10, 70 + scrollOffset);

    char inputText[1280];
    snprintf(inputText, sizeof(inputText), "Input: %s", inputBuffer);
    renderText(inputText, 10, 100 + scrollOffset);

    renderText("WASD to move, scroll to scroll, type to input text", 10, 130 + scrollOffset);
    renderText("ESC to quit", 10, 160 + scrollOffset);
}

void Engine::handleKeyboard(int key, int scancode, int action, int mods) {
    std::cout << "Key event: key=" << key << ", scancode=" << scancode << ", action=" << action << ", mods=" << mods << std::endl;
    
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_ESCAPE) {
            std::cout << "ESC pressed - closing window" << std::endl;
            glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
        } else if (key == GLFW_KEY_BACKSPACE && inputLength > 0) {
            std::cout << "Backspace pressed - removing character" << std::endl;
            inputBuffer[--inputLength] = '\0';
        } else if (key == GLFW_KEY_ENTER && inputLength < sizeof(inputBuffer) - 1) {
            std::cout << "Enter pressed - adding newline" << std::endl;
            inputBuffer[inputLength++] = '\n';
            inputBuffer[inputLength] = '\0';
        } else if (key == GLFW_KEY_SPACE && inputLength < sizeof(inputBuffer) - 1) {
            std::cout << "Space pressed - adding space" << std::endl;
            inputBuffer[inputLength++] = ' ';
            inputBuffer[inputLength] = '\0';
        } else if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z && inputLength < sizeof(inputBuffer) - 1) {
            char ch = 'a' + (key - GLFW_KEY_A);
            if (mods & GLFW_MOD_SHIFT) {
                ch = 'A' + (key - GLFW_KEY_A);
            }
            std::cout << "Letter pressed: " << ch << std::endl;
            inputBuffer[inputLength++] = ch;
            inputBuffer[inputLength] = '\0';
        } else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9 && inputLength < sizeof(inputBuffer) - 1) {
            char digit = '0' + (key - GLFW_KEY_0);
            std::cout << "Number pressed: " << digit << std::endl;
            inputBuffer[inputLength++] = digit;
            inputBuffer[inputLength] = '\0';
        } else {
            std::cout << "Unhandled key: " << key << std::endl;
        }
        
        std::cout << "Input buffer now: '" << inputBuffer << "' (length: " << inputLength << ")" << std::endl;
    }
}

void Engine::handleScroll(double xoffset, double yoffset) {
    scrollOffset += yoffset * 10.0f;
}

void Engine::handleMouse(int button, int action, int mods, double x, double y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        char mouseText[64];
        snprintf(mouseText, sizeof(mouseText), "Click: %.0f,%.0f", x, y);
        if (inputLength + strlen(mouseText) < sizeof(inputBuffer) - 1) {
            strcat(inputBuffer, mouseText);
            inputLength += strlen(mouseText);
        }
    }
}

bool Engine::initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Could not init FreeType Library" << std::endl;
        return false;
    }
    return true;
}

bool Engine::loadFont(const char* fontPath) {
    if (FT_New_Face(ft, fontPath, 0, &face)) {
        return false; // Failed to load font
    }
    
    // Set size to load glyphs as (48 pixel height)
    FT_Set_Pixel_Sizes(face, 0, 24);
    
    fontLoaded = true;
    
    // Load basic ASCII characters
    for (unsigned char c = 32; c < 127; c++) {
        loadGlyph(c);
    }
    
    return true;
}

void Engine::loadGlyph(char c) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
        std::cerr << "Failed to load glyph for: " << c << std::endl;
        return;
    }

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_ALPHA,
        face->glyph->bitmap.width,
        face->glyph->bitmap.rows,
        0,
        GL_ALPHA,
        GL_UNSIGNED_BYTE,
        face->glyph->bitmap.buffer
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Glyph glyph = {
        texture,
        (int)face->glyph->bitmap.width,
        (int)face->glyph->bitmap.rows,
        (int)face->glyph->bitmap_left,
        (int)face->glyph->bitmap_top,
        (int)(face->glyph->advance.x >> 6)
    };

    glyphs[c] = glyph;
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Engine::renderChar(char c, float x, float y) {
    if (glyphs.find(c) == glyphs.end()) {
        return;
    }
    
    Glyph& g = glyphs[c];
    
    float xpos = x + g.bearingX;
    float ypos = y - g.bearingY;  // Simple baseline calculation
    float w = g.width;
    float h = g.height;
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g.textureID);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos, ypos + h);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + w, ypos + h); 
        glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + w, ypos);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos, ypos);
    glEnd();
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void Engine::renderText(const char* text, float x, float y) {
    float currentX = x;
    float currentY = y;
    
    for (const char* p = text; *p; p++) {
        if (*p == '\n') {
            currentY += 24;
            currentX = x;
            continue;
        }
        
        if (glyphs.find(*p) != glyphs.end()) {
            renderChar(*p, currentX, currentY);
            currentX += glyphs[*p].advance;
        }
    }
}