#include "engine.h"
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

Engine::Engine() : leftMouseHeld(false), scrollOffset(0.0f), scrollVelocity(0.0f),
                   contentHeight(0.0f), viewportHeight(0), inputBuffer(nullptr), inputLength(0),
                   fontLoaded(false), cursorPos(0), selectionStart(0), selectionEnd(0), hasSelection(false),
                   caretAnimX(0), caretAnimY(0), caretVelX(0), caretVelY(0),
                   caretTargetX(0), caretTargetY(0), lastBlinkTime(0), caretVisible(true) {
    // Allocate 10MB input buffer
    inputBuffer = new char[INPUT_BUFFER_SIZE];
    memset(inputBuffer, 0, INPUT_BUFFER_SIZE);

    // Initialize markdown rendering system
    markdownRenderer = std::make_unique<MarkdownRenderer>();
    textBuffer = std::make_unique<TextBuffer>();

    // Set up initial markdown content with enough to scroll
    std::string initialContent =
        "# Welcome to Markdown Editor\n\n"
        "This is a paragraph of body text that demonstrates word wrapping.\n\n"
        "Here is another paragraph with more content.\n\n"
        "## Getting Started\n\n"
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n\n"
        "Sed do eiusmod tempor incididunt ut labore et dolore.\n\n"
        "## Features\n\n"
        "Duis aute irure dolor in reprehenderit in voluptate.\n\n"
        "Excepteur sint occaecat cupidatat non proident.\n\n"
        "## More Content\n\n"
        "Sunt in culpa qui officia deserunt mollit anim.\n\n"
        "Ut enim ad minim veniam quis nostrud exercitation.\n\n"
        "## Final Section\n\n"
        "At vero eos et accusamus et iusto odio dignissimos.\n\n"
        "End of document.";

    // Copy to input buffer for editing
    strncpy(inputBuffer, initialContent.c_str(), INPUT_BUFFER_SIZE - 1);
    inputLength = initialContent.length();
    cursorPos = 0;

    textBuffer->setText(initialContent);
    markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
}

Engine::~Engine() {
    delete[] inputBuffer;

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
    viewportHeight = height;

    // Simple momentum scrolling
    float maxScroll = std::max(0.0f, contentHeight - height);

    scrollVelocity *= 0.9f;
    scrollOffset += scrollVelocity;

    // Hard clamp
    if (scrollOffset < 0) {
        scrollOffset = 0;
        scrollVelocity = 0;
    }
    if (scrollOffset > maxScroll) {
        scrollOffset = maxScroll;
        scrollVelocity = 0;
    }

    glViewport(0, 0, width, height);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Apply scroll offset
    glTranslatef(0, -scrollOffset, 0);

    if (markdownRenderer) {
        int domCursorPos = markdownRenderer->rawToDOM(cursorPos);
        int domSelStart = markdownRenderer->rawToDOM(selectionStart);
        int domSelEnd = markdownRenderer->rawToDOM(selectionEnd);

        // Update cursor blink (530ms cycle)
        double currentTime = glfwGetTime();
        if (currentTime - lastBlinkTime > 0.53) {
            caretVisible = !caretVisible;
            lastBlinkTime = currentTime;
        }

        CaretState caret;
        caret.cursorPosition = domCursorPos;
        caret.selectionStart = domSelStart;
        caret.selectionEnd = domSelEnd;
        caret.hasSelection = hasSelection;
        caret.caretVisible = caretVisible && !hasSelection;
        caret.animatedCaretX = caretAnimX;
        caret.animatedCaretY = caretAnimY;
        caret.useAnimatedPosition = true;
        markdownRenderer->setCaretState(caret);

        Size viewportSize(width, height);
        markdownRenderer->render(viewportSize);

        contentHeight = markdownRenderer->getContentHeight();

        // Update animated cursor position (lerp toward target)
        updateCaretAnimation();
    }

    // Reset transform for scrollbar (fixed position UI)
    glLoadIdentity();

    // Draw scrollbar if content is taller than viewport
    if (contentHeight > height) {
        float scrollbarWidth = 7.0f;
        float margin = 3.0f;
        float trackX = width - scrollbarWidth - margin;
        float trackHeight = height - margin * 2;

        // Thumb size - shrinks when overscrolling
        float visibleRatio = (float)height / contentHeight;
        float baseThumbHeight = std::max(40.0f, trackHeight * visibleRatio);
        float thumbHeight = baseThumbHeight;

        // Shrink thumb during overscroll
        float overscroll = std::max(-scrollOffset, scrollOffset - maxScroll);
        if (overscroll > 0) {
            float shrinkFactor = 1.0f / (1.0f + overscroll * 0.005f);
            thumbHeight = baseThumbHeight * shrinkFactor;
            thumbHeight = std::max(20.0f, thumbHeight);
        }

        // Thumb position - clamp ratio but allow visual feedback
        float scrollRatio = (maxScroll > 0) ? scrollOffset / maxScroll : 0;
        float thumbY;
        if (scrollRatio < 0) {
            // Overscrolled past top - thumb stays at top but shrinks
            thumbY = margin;
        } else if (scrollRatio > 1) {
            // Overscrolled past bottom - thumb stays at bottom
            thumbY = margin + trackHeight - thumbHeight;
        } else {
            thumbY = margin + scrollRatio * (trackHeight - thumbHeight);
        }

        glDisable(GL_TEXTURE_2D);

        // Thumb with rounded appearance
        float radius = scrollbarWidth / 2.0f;
        float centerX = trackX + radius;

        glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
        glBegin(GL_QUADS);
        glVertex2f(trackX, thumbY + radius);
        glVertex2f(trackX + scrollbarWidth, thumbY + radius);
        glVertex2f(trackX + scrollbarWidth, thumbY + thumbHeight - radius);
        glVertex2f(trackX, thumbY + thumbHeight - radius);
        glEnd();

        // Top cap
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(centerX, thumbY + radius);
        for (int i = 0; i <= 12; i++) {
            float angle = 3.14159f + (3.14159f * i / 12);
            glVertex2f(centerX + radius * cos(angle), thumbY + radius + radius * sin(angle));
        }
        glEnd();

        // Bottom cap
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(centerX, thumbY + thumbHeight - radius);
        for (int i = 0; i <= 12; i++) {
            float angle = (3.14159f * i / 12);
            glVertex2f(centerX + radius * cos(angle), thumbY + thumbHeight - radius + radius * sin(angle));
        }
        glEnd();
    }
}

void Engine::handleKeyboard(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        bool shift = mods & GLFW_MOD_SHIFT;
        bool alt = mods & GLFW_MOD_ALT;
        // Platform-agnostic: CMD on macOS, CTRL on Windows/Linux
        bool cmdOrCtrl = (mods & GLFW_MOD_SUPER) || (mods & GLFW_MOD_CONTROL);

        // Handle keyboard shortcuts (Ctrl/Cmd + key)
        if (cmdOrCtrl) {
            if (key == GLFW_KEY_A) {
                selectAll();
                return;
            } else if (key == GLFW_KEY_C) {
                copySelection();
                return;
            } else if (key == GLFW_KEY_V) {
                paste();
                return;
            } else if (key == GLFW_KEY_Z) {
                undo();
                return;
            }
        }

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
            if (cmdOrCtrl) {
                // Cmd/Ctrl+Up: go to start of document
                if (shift && !hasSelection) {
                    selectionStart = cursorPos;
                    hasSelection = true;
                }
                cursorPos = 0;
                if (shift) selectionEnd = cursorPos;
                else hasSelection = false;
                scrollOffset = 0;  // Scroll to top
            } else {
                moveCursorVertically(-1, shift);
            }

        } else if (key == GLFW_KEY_DOWN) {
            if (cmdOrCtrl) {
                // Cmd/Ctrl+Down: go to end of document
                if (shift && !hasSelection) {
                    selectionStart = cursorPos;
                    hasSelection = true;
                }
                cursorPos = inputLength;
                if (shift) selectionEnd = cursorPos;
                else hasSelection = false;
                // Scroll to bottom
                float maxScroll = std::max(0.0f, contentHeight - viewportHeight);
                scrollOffset = maxScroll;
            } else {
                moveCursorVertically(1, shift);
            }

        } else if (key == GLFW_KEY_BACKSPACE) {
            if (hasSelection) {
                // Delete selection
                saveUndoState();
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
            } else if (cmdOrCtrl && cursorPos > 0) {
                // Cmd/Ctrl+Backspace: delete to start of line
                saveUndoState();
                int lineStart = findLineStart(cursorPos);
                if (lineStart < cursorPos) {
                    // Delete from cursor to start of line
                    int deleteCount = cursorPos - lineStart;
                    memmove(inputBuffer + lineStart, inputBuffer + cursorPos, inputLength - cursorPos + 1);
                    inputLength -= deleteCount;
                    cursorPos = lineStart;
                } else if (cursorPos > 0) {
                    // Already at start of line - delete the newline to merge with previous line
                    memmove(inputBuffer + cursorPos - 1, inputBuffer + cursorPos, inputLength - cursorPos + 1);
                    inputLength--;
                    cursorPos--;
                }

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
                saveUndoState();
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
                saveUndoState();
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
    (void)xoffset;
    scrollVelocity += -yoffset * 15.0f;
}

void Engine::handleMouse(int button, int action, int mods, double x, double y) {
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftMouseHeld = true;
            if (markdownRenderer) {
                // Add scroll offset to get content-space y coordinate
                float contentY = static_cast<float>(y) + scrollOffset;
                cursorPos = markdownRenderer->hitTest(static_cast<float>(x), contentY);
                cursorPos = std::max(0, std::min(cursorPos, inputLength));
                selectionStart = cursorPos;
                selectionEnd = cursorPos;
                hasSelection = false;
            }
        } else if (action == GLFW_RELEASE) {
            leftMouseHeld = false;
        }
    }
}

void Engine::handleMouseMove(double x, double y) {
    if (leftMouseHeld && markdownRenderer) {
        float contentY = static_cast<float>(y) + scrollOffset;
        int newPos = markdownRenderer->hitTest(static_cast<float>(x), contentY);
        newPos = std::max(0, std::min(newPos, inputLength));
        cursorPos = newPos;
        selectionEnd = newPos;
        hasSelection = (selectionStart != selectionEnd);
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

    // Reset blink timer
    lastBlinkTime = glfwGetTime();
    caretVisible = true;
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
    saveUndoState();

    if (hasSelection) {
        // Replace selection
        int start = std::min(selectionStart, selectionEnd);
        int end = std::max(selectionStart, selectionEnd);
        memmove(inputBuffer + start, inputBuffer + end, inputLength - end + 1);
        inputLength -= (end - start);
        cursorPos = start;
        hasSelection = false;
    }

    if (inputLength < INPUT_BUFFER_SIZE - 1) {
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
        ensureCursorVisible();

        // Reset blink timer so cursor is visible after typing
        lastBlinkTime = glfwGetTime();
        caretVisible = true;
    }
}

void Engine::deleteChar() {
    saveUndoState();

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
    saveUndoState();

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

void Engine::selectAll() {
    selectionStart = 0;
    selectionEnd = inputLength;
    cursorPos = inputLength;
    hasSelection = (inputLength > 0);
}

void Engine::copySelection() {
    if (!hasSelection) {
        return;
    }

    int start = std::min(selectionStart, selectionEnd);
    int end = std::max(selectionStart, selectionEnd);
    std::string selectedText(inputBuffer + start, end - start);
    Clipboard::setText(selectedText);
}

void Engine::paste() {
    std::string clipboardText = Clipboard::getText();
    if (clipboardText.empty()) {
        return;
    }

    saveUndoState();

    // Delete selection if present
    if (hasSelection) {
        int start = std::min(selectionStart, selectionEnd);
        int end = std::max(selectionStart, selectionEnd);
        memmove(inputBuffer + start, inputBuffer + end, inputLength - end + 1);
        inputLength -= (end - start);
        cursorPos = start;
        hasSelection = false;
    }

    // Insert clipboard text
    int pasteLen = clipboardText.length();
    if (inputLength + pasteLen < INPUT_BUFFER_SIZE - 1) {
        memmove(inputBuffer + cursorPos + pasteLen, inputBuffer + cursorPos, inputLength - cursorPos + 1);
        memcpy(inputBuffer + cursorPos, clipboardText.c_str(), pasteLen);
        inputLength += pasteLen;
        cursorPos += pasteLen;

        // Update markdown content
        if (textBuffer && markdownRenderer) {
            std::string newText(inputBuffer, inputLength);
            textBuffer->setText(newText);
            markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
        }
        ensureCursorVisible();
    }
}

void Engine::ensureCursorVisible() {
    if (!markdownRenderer) return;

    int domPos = markdownRenderer->rawToDOM(cursorPos);
    float cursorY = markdownRenderer->getCursorY(domPos);

    float margin = 50.0f;

    // If cursor is below visible area, scroll down
    if (cursorY > scrollOffset + viewportHeight - margin) {
        scrollOffset = cursorY - viewportHeight + margin;
    }
    // If cursor is above visible area, scroll up
    else if (cursorY - 40 < scrollOffset + margin) {
        scrollOffset = std::max(0.0f, cursorY - 40 - margin);
    }

    // Clamp scroll offset
    float maxScroll = std::max(0.0f, contentHeight - viewportHeight);
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
}

void Engine::updateCaretAnimation() {
    if (!markdownRenderer) return;

    // Get target position from renderer
    int domPos = markdownRenderer->rawToDOM(cursorPos);
    markdownRenderer->getCursorXY(domPos, caretTargetX, caretTargetY);

    // Simple smooth lerp
    float t = 0.4f;
    caretAnimX += (caretTargetX - caretAnimX) * t;
    caretAnimY += (caretTargetY - caretAnimY) * t;

    // Snap when close
    if (std::abs(caretTargetX - caretAnimX) < 0.5f) caretAnimX = caretTargetX;
    if (std::abs(caretTargetY - caretAnimY) < 0.5f) caretAnimY = caretTargetY;
}

void Engine::saveUndoState() {
    UndoState state;
    state.text = std::string(inputBuffer, inputLength);
    state.cursorPos = cursorPos;

    undoStack.push_back(state);

    // Limit undo stack size
    if (undoStack.size() > MAX_UNDO) {
        undoStack.erase(undoStack.begin());
    }
}

void Engine::undo() {
    if (undoStack.empty()) return;

    UndoState state = undoStack.back();
    undoStack.pop_back();

    // Restore state
    memcpy(inputBuffer, state.text.c_str(), state.text.length());
    inputBuffer[state.text.length()] = '\0';
    inputLength = state.text.length();
    cursorPos = std::min(state.cursorPos, inputLength);
    hasSelection = false;

    // Update markdown content
    if (textBuffer && markdownRenderer) {
        textBuffer->setText(state.text);
        markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
    }

    // Reset blink
    lastBlinkTime = glfwGetTime();
    caretVisible = true;
}

