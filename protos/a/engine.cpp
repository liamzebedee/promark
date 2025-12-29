#include "engine.h"
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>

Engine::Engine() : scrollOffset(0.0f), inputLength(0) {
    memset(inputBuffer, 0, sizeof(inputBuffer));
    initFont();
}

Engine::~Engine() {
}

bool Engine::initialize() {
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
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

void Engine::initFont() {
    memset(fontData, 0, sizeof(fontData));
    
    // Simple 8x8 bitmap font for basic ASCII characters
    unsigned char charPatterns[][8] = {
        // Space (32)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        // ! (33)
        {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},
        // " (34)
        {0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00},
        // # (35)
        {0x66, 0xFF, 0x66, 0x66, 0xFF, 0x66, 0x00, 0x00},
        // ... continuing with more characters
        // A (65)
        {0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
        // B (66)
        {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
        // C (67)
        {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    };
    
    // Copy patterns to font data (starting from space character 32)
    for (int i = 0; i < 3; i++) {
        memcpy(fontData[32 + i], charPatterns[i], 8);
    }
    // A, B, C
    memcpy(fontData[65], charPatterns[4], 8);
    memcpy(fontData[66], charPatterns[5], 8);
    memcpy(fontData[67], charPatterns[6], 8);
    
    // Simple patterns for digits and letters
    for (int c = 48; c <= 57; c++) { // 0-9
        for (int row = 0; row < 8; row++) {
            fontData[c][row] = 0x3C + (row % 4) * 8;
        }
    }
    
    for (int c = 97; c <= 122; c++) { // a-z
        for (int row = 0; row < 8; row++) {
            fontData[c][row] = 0x18 + (row % 3) * 16;
        }
    }
}

void Engine::renderChar(char c, float x, float y) {
    if (c < 0 || c >= 128) return;
    
    glColor3f(0.0f, 0.0f, 0.0f); // Black text
    
    for (int row = 0; row < 8; row++) {
        unsigned char rowData = fontData[(unsigned char)c][row];
        for (int col = 0; col < 8; col++) {
            if (rowData & (0x80 >> col)) {
                float px = x + col;
                float py = y + row;
                
                glBegin(GL_QUADS);
                glVertex2f(px, py);
                glVertex2f(px + 1, py);
                glVertex2f(px + 1, py + 1);
                glVertex2f(px, py + 1);
                glEnd();
            }
        }
    }
}

void Engine::renderText(const char* text, float x, float y) {
    float charWidth = 8.0f;
    float lineHeight = 12.0f;
    float currentX = x;
    float currentY = y;
    
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            currentY += lineHeight;
            currentX = x;
        } else {
            renderChar(text[i], currentX, currentY);
            currentX += charWidth;
        }
    }
}