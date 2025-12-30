#include "engine.h"
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <algorithm>

Engine::Engine() : scrollOffset(0.0f), inputLength(0), fontLoaded(false), 
                   cursorPos(0), selectionStart(0), selectionEnd(0), hasSelection(false) {
    memset(inputBuffer, 0, sizeof(inputBuffer));
    
    // Initialize markdown rendering system
    markdownRenderer = std::make_unique<MarkdownRenderer>();
    textBuffer = std::make_unique<TextBuffer>();
    
    // Set up initial markdown content - includes a 64x64 red square PNG test image
    std::string initialContent = "# Welcome to Markdown Editor\n\nThis is a paragraph of body text.\n\n![red square](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAIAAAAlC+aJAAAAeUlEQVR4nO3PQQkAMAzAwGqqfykTMxF7HINABFzm7H7dcEEDWtCAFjSgBQ1oQQNa0IAWNKAFDWhBA1rQgBY0oAUNaEEDWtCAFjSgBQ1oQQNa0IAWNKAFDWhBA1rQgBY0oAUNaEEDWtCAFjSgBQ1oQQNa0IAWNKAFj10YCQEtRI8KmQAAAABJRU5ErkJggg==)\n\nMore text after the image.";
    
    // Copy to input buffer for editing
    strncpy(inputBuffer, initialContent.c_str(), sizeof(inputBuffer) - 1);
    inputLength = initialContent.length();
    cursorPos = 0;  // Start at beginning
    
    textBuffer->setText(initialContent);
    markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
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

    // Pass font face to markdown renderer for glyph metrics
    markdownRenderer->setFontFace(face);

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
    glViewport(0, 0, width, height);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // White background
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (markdownRenderer) {
        // Convert raw cursor position to DOM position for display
        int domCursorPos = markdownRenderer->rawToDOM(cursorPos);
        int domSelStart = markdownRenderer->rawToDOM(selectionStart);
        int domSelEnd = markdownRenderer->rawToDOM(selectionEnd);

        CaretState caret;
        caret.cursorPosition = domCursorPos;
        caret.selectionStart = domSelStart;
        caret.selectionEnd = domSelEnd;
        caret.hasSelection = hasSelection;
        markdownRenderer->setCaretState(caret);

        Size viewportSize(width, height);
        markdownRenderer->render(viewportSize);
    }
}

void Engine::handleKeyboard(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        bool shift = mods & GLFW_MOD_SHIFT;
        bool alt = mods & GLFW_MOD_ALT;
        
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
            
        } else if (key == GLFW_KEY_LEFT) {
            if (alt) {
                moveCursorByWord(-1, shift);
            } else {
                moveCursor(-1, shift);
            }
            
        } else if (key == GLFW_KEY_RIGHT) {
            if (alt) {
                moveCursorByWord(1, shift);
            } else {
                moveCursor(1, shift);
            }

        } else if (key == GLFW_KEY_UP) {
            moveCursorVertically(-1, shift);

        } else if (key == GLFW_KEY_DOWN) {
            moveCursorVertically(1, shift);

        } else if (key == GLFW_KEY_BACKSPACE) {
            if (hasSelection) {
                // Delete selection
                int start = std::min(selectionStart, selectionEnd);
                int end = std::max(selectionStart, selectionEnd);
                memmove(inputBuffer + start, inputBuffer + end, inputLength - end + 1);
                inputLength -= (end - start);
                cursorPos = start;
                hasSelection = false;

                // Update markdown content
                if (textBuffer && markdownRenderer) {
                    std::string newText(inputBuffer, inputLength);
                    textBuffer->setText(newText);
                    markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
                }
            } else if (alt && cursorPos > 0) {
                // Alt+Backspace: delete word backwards
                deleteWordBackward();
            } else if (cursorPos > 0) {
                deleteChar();
            }
            
        } else if (key == GLFW_KEY_DELETE) {
            if (hasSelection) {
                // Delete selection
                int start = std::min(selectionStart, selectionEnd);
                int end = std::max(selectionStart, selectionEnd);
                memmove(inputBuffer + start, inputBuffer + end, inputLength - end + 1);
                inputLength -= (end - start);
                cursorPos = start;
                hasSelection = false;

                // Update markdown content
                if (textBuffer && markdownRenderer) {
                    std::string newText(inputBuffer, inputLength);
                    textBuffer->setText(newText);
                    markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
                }
            } else if (cursorPos < inputLength) {
                memmove(inputBuffer + cursorPos, inputBuffer + cursorPos + 1, inputLength - cursorPos);
                inputLength--;
                inputBuffer[inputLength] = '\0';

                // Update markdown content
                if (textBuffer && markdownRenderer) {
                    std::string newText(inputBuffer, inputLength);
                    textBuffer->setText(newText);
                    markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
                }
            }
            
        } else if (key == GLFW_KEY_HOME) {
            cursorPos = 0;
            if (!shift) hasSelection = false;
            else {
                if (!hasSelection) selectionStart = cursorPos;
                selectionEnd = cursorPos;
                hasSelection = true;
            }
            
        } else if (key == GLFW_KEY_END) {
            cursorPos = inputLength;
            if (!shift) hasSelection = false;
            else {
                if (!hasSelection) selectionStart = cursorPos;
                selectionEnd = cursorPos;
                hasSelection = true;
            }
            
        } else if (key == GLFW_KEY_ENTER) {
            insertChar('\n');
            
        } else if (key == GLFW_KEY_SPACE) {
            insertChar(' ');
            
        } else if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
            char ch = shift ? ('A' + (key - GLFW_KEY_A)) : ('a' + (key - GLFW_KEY_A));
            insertChar(ch);
            
        } else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
            char ch;
            if (shift) {
                const char* shiftChars = ")!@#$%^&*(";
                ch = shiftChars[key - GLFW_KEY_0];
            } else {
                ch = '0' + (key - GLFW_KEY_0);
            }
            insertChar(ch);
            
        } else {
            // Handle special characters
            char ch = 0;
            if (key == GLFW_KEY_SEMICOLON) ch = shift ? ':' : ';';
            else if (key == GLFW_KEY_APOSTROPHE) ch = shift ? '"' : '\'';
            else if (key == GLFW_KEY_COMMA) ch = shift ? '<' : ',';
            else if (key == GLFW_KEY_PERIOD) ch = shift ? '>' : '.';
            else if (key == GLFW_KEY_SLASH) ch = shift ? '?' : '/';
            else if (key == GLFW_KEY_GRAVE_ACCENT) ch = shift ? '~' : '`';
            else if (key == GLFW_KEY_LEFT_BRACKET) ch = shift ? '{' : '[';
            else if (key == GLFW_KEY_RIGHT_BRACKET) ch = shift ? '}' : ']';
            else if (key == GLFW_KEY_BACKSLASH) ch = shift ? '|' : '\\';
            else if (key == GLFW_KEY_MINUS) ch = shift ? '_' : '-';
            else if (key == GLFW_KEY_EQUAL) ch = shift ? '+' : '=';
            
            if (ch != 0) {
                insertChar(ch);
            }
        }
    }
}

void Engine::handleScroll(double xoffset, double yoffset) {
    scrollOffset += yoffset * 10.0f;
}

void Engine::handleMouse(int button, int action, int mods, double x, double y) {
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (markdownRenderer) {
            // Convert click position to raw cursor position
            cursorPos = markdownRenderer->hitTest(static_cast<float>(x), static_cast<float>(y));
            cursorPos = std::max(0, std::min(cursorPos, inputLength));
            hasSelection = false;
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

void Engine::moveCursor(int delta, bool extendSelection) {
    int newPos = cursorPos + delta;
    newPos = std::max(0, std::min(newPos, inputLength));
    
    if (extendSelection) {
        if (!hasSelection) {
            selectionStart = cursorPos;
            hasSelection = true;
        }
        selectionEnd = newPos;
    } else {
        hasSelection = false;
    }
    
    cursorPos = newPos;
}

void Engine::moveCursorByWord(int direction, bool extendSelection) {
    int newPos = findWordBoundary(cursorPos, direction);
    
    if (extendSelection) {
        if (!hasSelection) {
            selectionStart = cursorPos;
            hasSelection = true;
        }
        selectionEnd = newPos;
    } else {
        hasSelection = false;
    }
    
    cursorPos = newPos;
}

int Engine::findWordBoundary(int pos, int direction) {
    if (direction > 0) {
        // Move forward to next word
        while (pos < inputLength && inputBuffer[pos] != ' ' && inputBuffer[pos] != '\n') pos++;
        while (pos < inputLength && (inputBuffer[pos] == ' ' || inputBuffer[pos] == '\n')) pos++;
    } else {
        // Move backward to previous word
        while (pos > 0 && (inputBuffer[pos-1] == ' ' || inputBuffer[pos-1] == '\n')) pos--;
        while (pos > 0 && inputBuffer[pos-1] != ' ' && inputBuffer[pos-1] != '\n') pos--;
    }
    return pos;
}

void Engine::insertChar(char c) {
    if (hasSelection) {
        // Replace selection
        int start = std::min(selectionStart, selectionEnd);
        int end = std::max(selectionStart, selectionEnd);
        memmove(inputBuffer + start, inputBuffer + end, inputLength - end + 1);
        inputLength -= (end - start);
        cursorPos = start;
        hasSelection = false;
    }
    
    if (inputLength < sizeof(inputBuffer) - 1) {
        memmove(inputBuffer + cursorPos + 1, inputBuffer + cursorPos, inputLength - cursorPos + 1);
        inputBuffer[cursorPos] = c;
        inputLength++;
        cursorPos++;
        
        // Update markdown content
        if (textBuffer && markdownRenderer) {
            std::string newText(inputBuffer, inputLength);
            textBuffer->setText(newText);
            markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
        }
    }
}

void Engine::deleteChar() {
    if (cursorPos > 0) {
        memmove(inputBuffer + cursorPos - 1, inputBuffer + cursorPos, inputLength - cursorPos + 1);
        inputLength--;
        cursorPos--;
        
        // Update markdown content
        if (textBuffer && markdownRenderer) {
            std::string newText(inputBuffer, inputLength);
            textBuffer->setText(newText);
            markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
        }
    }
}

void Engine::deleteWordBackward() {
    if (cursorPos > 0) {
        int wordStart = findWordBoundary(cursorPos, -1);
        int deleteCount = cursorPos - wordStart;

        if (deleteCount > 0) {
            memmove(inputBuffer + wordStart, inputBuffer + cursorPos, inputLength - cursorPos + 1);
            inputLength -= deleteCount;
            cursorPos = wordStart;

            if (textBuffer && markdownRenderer) {
                std::string newText(inputBuffer, inputLength);
                textBuffer->setText(newText);
                markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
            }
        }
    }
}

int Engine::findLineStart(int pos) {
    while (pos > 0 && inputBuffer[pos - 1] != '\n') {
        pos--;
    }
    return pos;
}

int Engine::findLineEnd(int pos) {
    while (pos < inputLength && inputBuffer[pos] != '\n') {
        pos++;
    }
    return pos;
}

int Engine::getColumnInLine(int pos) {
    int lineStart = findLineStart(pos);
    return pos - lineStart;
}

int Engine::findPositionInLine(int lineStart, int column) {
    int lineEnd = findLineEnd(lineStart);
    int lineLength = lineEnd - lineStart;
    if (column > lineLength) column = lineLength;
    return lineStart + column;
}

void Engine::moveCursorVertically(int direction, bool extendSelection) {
    int currentLineStart = findLineStart(cursorPos);
    int column = getColumnInLine(cursorPos);
    int newPos = cursorPos;

    if (direction < 0) {
        // Move up
        if (currentLineStart > 0) {
            int prevLineEnd = currentLineStart - 1;
            int prevLineStart = findLineStart(prevLineEnd);
            newPos = findPositionInLine(prevLineStart, column);
        }
    } else {
        // Move down
        int currentLineEnd = findLineEnd(cursorPos);
        if (currentLineEnd < inputLength) {
            int nextLineStart = currentLineEnd + 1;
            newPos = findPositionInLine(nextLineStart, column);
        }
    }

    if (extendSelection) {
        if (!hasSelection) {
            selectionStart = cursorPos;
            hasSelection = true;
        }
        selectionEnd = newPos;
    } else {
        hasSelection = false;
    }

    cursorPos = newPos;
}

