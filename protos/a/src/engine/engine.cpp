#include "engine.h"
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

Engine::Engine() : wantsToClose(false), leftMouseHeld(false), dirty(false), lastClickTime(0), lastClickX(0), lastClickY(0), clickCount(0),
                   scrollOffset(0.0f), scrollVelocity(0.0f),
                   contentHeight(0.0f), viewportHeight(0), inputBuffer(nullptr), inputLength(0),
                   fontLoaded(false), cursorPos(0), goalColumn(0), selectionStart(0), selectionEnd(0), hasSelection(false),
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
        if (monoFace) {
            FT_Done_Face(monoFace);
        }
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

    // Load monospace font for code blocks
    const char* monoFontPaths[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.dfont",
        "/System/Library/Fonts/Courier.dfont",
        "/Library/Fonts/Courier New.ttf"
    };

    monoFace = nullptr;
    for (const char* monoPath : monoFontPaths) {
        if (FT_New_Face(ft, monoPath, 0, &monoFace) == 0) {
            FT_Set_Pixel_Sizes(monoFace, 0, 24);
            std::cout << "Loaded mono font: " << monoPath << std::endl;
            break;
        }
    }

    // Pass font faces to markdown renderer for glyph metrics
    markdownRenderer->setFontFace(face);
    if (monoFace) {
        markdownRenderer->setMonoFontFace(monoFace);
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
    viewportHeight = height - TOOLBAR_HEIGHT;
    int contentAreaHeight = height - TOOLBAR_HEIGHT;

    // macOS-style momentum scrolling
    float maxScroll = std::max(0.0f, contentHeight - contentAreaHeight);

    // Natural deceleration: friction increases as velocity decreases
    float absVel = std::abs(scrollVelocity);
    float friction;
    if (absVel > 50.0f) {
        friction = 0.95f;  // High speed: gentle friction
    } else if (absVel > 10.0f) {
        friction = 0.92f;  // Medium speed: moderate friction
    } else if (absVel > 1.0f) {
        friction = 0.85f;  // Low speed: stronger friction
    } else {
        friction = 0.0f;   // Stop completely when very slow
        scrollVelocity = 0;
    }
    scrollVelocity *= friction;
    scrollOffset += scrollVelocity;

    // Rubber-banding at edges
    float overscrollTop = -scrollOffset;
    float overscrollBottom = scrollOffset - maxScroll;

    if (overscrollTop > 0) {
        // Overscrolled past top - spring back
        float springForce = overscrollTop * 0.15f;
        scrollVelocity += springForce;
        // Dampen velocity when overscrolled
        if (scrollVelocity < 0) {
            scrollVelocity *= 0.7f;
        }
    } else if (overscrollBottom > 0) {
        // Overscrolled past bottom - spring back
        float springForce = overscrollBottom * 0.15f;
        scrollVelocity -= springForce;
        // Dampen velocity when overscrolled
        if (scrollVelocity > 0) {
            scrollVelocity *= 0.7f;
        }
    }

    // Soft clamp - allow slight overscroll but limit it
    float maxOverscroll = 100.0f;
    if (scrollOffset < -maxOverscroll) {
        scrollOffset = -maxOverscroll;
        scrollVelocity = 0;
    }
    if (scrollOffset > maxScroll + maxOverscroll) {
        scrollOffset = maxScroll + maxOverscroll;
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

    // Render toolbar at top (fixed position)
    renderToolbar(width);

    // Set up clipping for content area (below toolbar)
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, width, height - TOOLBAR_HEIGHT);

    // Apply scroll offset and toolbar offset
    glTranslatef(0, TOOLBAR_HEIGHT - scrollOffset, 0);

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

        Size viewportSize(width, contentAreaHeight);
        markdownRenderer->render(viewportSize);

        contentHeight = markdownRenderer->getContentHeight();

        // Update animated cursor position (lerp toward target)
        updateCaretAnimation();
    }

    // Disable scissor test before drawing fixed UI elements
    glDisable(GL_SCISSOR_TEST);

    // Reset transform for scrollbar (fixed position UI)
    glLoadIdentity();

    // Draw scrollbar if content is taller than content area
    if (contentHeight > contentAreaHeight) {
        float scrollbarWidth = 7.0f;
        float margin = 3.0f;
        float trackX = width - scrollbarWidth - margin;
        float trackTop = TOOLBAR_HEIGHT + margin;
        float trackHeight = height - TOOLBAR_HEIGHT - margin * 2;

        // Thumb size - shrinks when overscrolling
        float visibleRatio = (float)contentAreaHeight / contentHeight;
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
            thumbY = trackTop;
        } else if (scrollRatio > 1) {
            // Overscrolled past bottom - thumb stays at bottom
            thumbY = trackTop + trackHeight - thumbHeight;
        } else {
            thumbY = trackTop + scrollRatio * (trackHeight - thumbHeight);
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
            if (key == GLFW_KEY_W) {
                wantsToClose = true;
                return;
            } else if (key == GLFW_KEY_A) {
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
            // Cancel selection, never close window
            hasSelection = false;
            return;
            
        } else if (key == GLFW_KEY_LEFT) {
            if (cmdOrCtrl) {
                // Cmd/Ctrl+Left: select to start of line
                int lineStart = findLineStart(cursorPos);
                if (!hasSelection) {
                    selectionStart = cursorPos;
                }
                selectionEnd = lineStart;
                hasSelection = (selectionStart != selectionEnd);
                cursorPos = lineStart;
                lastBlinkTime = glfwGetTime();
                caretVisible = true;
                ensureCursorVisible();
            } else if (alt) {
                moveCursorByWord(-1, shift);
            } else {
                moveCursor(-1, shift);
            }

        } else if (key == GLFW_KEY_RIGHT) {
            if (cmdOrCtrl) {
                // Cmd/Ctrl+Right: select to end of line
                int lineEnd = findLineEnd(cursorPos);
                if (!hasSelection) {
                    selectionStart = cursorPos;
                }
                selectionEnd = lineEnd;
                hasSelection = (selectionStart != selectionEnd);
                cursorPos = lineEnd;
                lastBlinkTime = glfwGetTime();
                caretVisible = true;
                ensureCursorVisible();
            } else if (alt) {
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
                goalColumn = getColumnInLine(cursorPos);
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
                goalColumn = getColumnInLine(cursorPos);

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
    // Lower multiplier for less sensitive scrolling (macOS-like feel)
    scrollVelocity += -yoffset * 6.0f;
}

void Engine::handleMouse(int button, int action, int mods, double x, double y) {
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // Check toolbar click first
            if (handleToolbarClick(x, y)) {
                return;
            }

            leftMouseHeld = true;
            if (markdownRenderer) {
                // Adjust y for toolbar and add scroll offset to get content-space y coordinate
                float contentY = static_cast<float>(y - TOOLBAR_HEIGHT) + scrollOffset;

                // Check if clicking on a link
                std::string linkUrl = markdownRenderer->getLinkAtPosition(static_cast<float>(x), contentY);
                if (!linkUrl.empty()) {
                    // Open link in browser
                    openUrl(linkUrl);
                    leftMouseHeld = false;
                    return;
                }

                // Detect multi-click (within 400ms and 5 pixels)
                double currentTime = glfwGetTime();
                bool isMultiClick = (currentTime - lastClickTime < 0.4) &&
                                    (std::abs(x - lastClickX) < 5) &&
                                    (std::abs(y - lastClickY) < 5);

                if (isMultiClick) {
                    clickCount = (clickCount % 3) + 1;  // Cycle 1->2->3->1
                } else {
                    clickCount = 1;
                }

                cursorPos = markdownRenderer->hitTest(static_cast<float>(x), contentY);
                cursorPos = std::max(0, std::min(cursorPos, inputLength));

                if (clickCount == 3) {
                    // Triple-click: select line
                    int lineStart = findLineStart(cursorPos);
                    int lineEnd = findLineEnd(cursorPos);
                    // Include the newline if present
                    if (lineEnd < inputLength && inputBuffer[lineEnd] == '\n') {
                        lineEnd++;
                    }
                    selectionStart = lineStart;
                    selectionEnd = lineEnd;
                    cursorPos = lineEnd;
                    hasSelection = (selectionStart != selectionEnd);
                    leftMouseHeld = false;  // Don't drag after triple-click
                } else if (clickCount == 2) {
                    // Double-click: select word
                    int wordStart = findWordBoundary(cursorPos, -1);
                    int wordEnd = findWordBoundary(cursorPos, 1);
                    selectionStart = wordStart;
                    selectionEnd = wordEnd;
                    cursorPos = wordEnd;
                    hasSelection = (selectionStart != selectionEnd);
                    leftMouseHeld = false;  // Don't drag after double-click
                } else {
                    // Single click
                    goalColumn = getColumnInLine(cursorPos);
                    selectionStart = cursorPos;
                    selectionEnd = cursorPos;
                    hasSelection = false;
                }

                lastClickTime = currentTime;
                lastClickX = x;
                lastClickY = y;
            }
        } else if (action == GLFW_RELEASE) {
            leftMouseHeld = false;
        }
    }
}

void Engine::openUrl(const std::string& url) {
    // macOS: use 'open' command to open URL in default browser
    std::string command = "open \"" + url + "\"";
    system(command.c_str());
}

void Engine::handleMouseMove(double x, double y) {
    if (leftMouseHeld && markdownRenderer) {
        // Adjust y for toolbar and add scroll offset
        float contentY = static_cast<float>(y - TOOLBAR_HEIGHT) + scrollOffset;
        int newPos = markdownRenderer->hitTest(static_cast<float>(x), contentY);
        newPos = std::max(0, std::min(newPos, inputLength));
        cursorPos = newPos;
        selectionEnd = newPos;
        hasSelection = (selectionStart != selectionEnd);
    }
}

bool Engine::isOverLink(double x, double y) {
    if (!markdownRenderer) return false;
    // Adjust y for toolbar and add scroll offset
    float contentY = static_cast<float>(y - TOOLBAR_HEIGHT) + scrollOffset;
    std::string url = markdownRenderer->getLinkAtPosition(static_cast<float>(x), contentY);
    return !url.empty();
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
    int newPos;

    if (extendSelection) {
        newPos = cursorPos + delta;
        newPos = std::max(0, std::min(newPos, inputLength));
        if (!hasSelection) {
            selectionStart = cursorPos;
            hasSelection = true;
        }
        selectionEnd = newPos;
    } else if (hasSelection) {
        // Cancel selection: jump to start or end of selection
        if (delta > 0) {
            // Right arrow: go to end of selection
            newPos = std::max(selectionStart, selectionEnd);
        } else {
            // Left arrow: go to start of selection
            newPos = std::min(selectionStart, selectionEnd);
        }
        hasSelection = false;
    } else {
        newPos = cursorPos + delta;
        newPos = std::max(0, std::min(newPos, inputLength));
    }

    cursorPos = newPos;
    goalColumn = getColumnInLine(cursorPos);

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
    goalColumn = getColumnInLine(cursorPos);  // Update goal column
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
        goalColumn = getColumnInLine(cursorPos);
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
        goalColumn = getColumnInLine(cursorPos);
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
            goalColumn = getColumnInLine(cursorPos);
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
    int newPos = cursorPos;

    if (direction < 0) {
        // Move up
        if (currentLineStart > 0) {
            int prevLineEnd = currentLineStart - 1;
            int prevLineStart = findLineStart(prevLineEnd);
            newPos = findPositionInLine(prevLineStart, goalColumn);
        }
    } else {
        // Move down
        int currentLineEnd = findLineEnd(cursorPos);
        if (currentLineEnd < inputLength) {
            int nextLineStart = currentLineEnd + 1;
            newPos = findPositionInLine(nextLineStart, goalColumn);
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

    // Reset blink timer and ensure cursor visible
    lastBlinkTime = glfwGetTime();
    caretVisible = true;
    ensureCursorVisible();
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

    dirty = true;
}

void Engine::setContent(const std::string& content) {
    size_t len = std::min(content.length(), (size_t)(INPUT_BUFFER_SIZE - 1));
    memcpy(inputBuffer, content.c_str(), len);
    inputBuffer[len] = '\0';
    inputLength = len;
    cursorPos = 0;
    goalColumn = 0;
    hasSelection = false;
    undoStack.clear();
    dirty = false;

    if (textBuffer && markdownRenderer) {
        textBuffer->setText(content);
        markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
    }
}

std::string Engine::getContent() const {
    return std::string(inputBuffer, inputLength);
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
    goalColumn = getColumnInLine(cursorPos);
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

void Engine::renderToolbar(int width) {
    glDisable(GL_TEXTURE_2D);

    // Draw toolbar background (light gray)
    glColor3f(0.95f, 0.95f, 0.95f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(width, 0);
    glVertex2f(width, TOOLBAR_HEIGHT);
    glVertex2f(0, TOOLBAR_HEIGHT);
    glEnd();

    // Draw bottom border
    glColor3f(0.8f, 0.8f, 0.8f);
    glBegin(GL_LINES);
    glVertex2f(0, TOOLBAR_HEIGHT);
    glVertex2f(width, TOOLBAR_HEIGHT);
    glEnd();

    // Button definitions: x position, width, label
    struct Button {
        float x;
        float w;
        const char* label;
    };

    float buttonHeight = 26.0f;
    float buttonY = (TOOLBAR_HEIGHT - buttonHeight) / 2.0f;
    float startX = 10.0f;
    float spacing = 8.0f;

    Button buttons[] = {
        {startX, 30, "B"},           // Bold
        {0, 24, "I"},                // Italic
        {0, 36, "H1"},               // Heading 1
        {0, 36, "H2"},               // Heading 2
        {0, 36, "H3"},               // Heading 3
        {0, 50, "Link"}              // Link
    };

    // Calculate positions
    float currentX = startX;
    for (int i = 0; i < 6; i++) {
        buttons[i].x = currentX;
        currentX += buttons[i].w + spacing;
    }

    // Draw buttons
    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < 6; i++) {
        Button& btn = buttons[i];

        // Button background
        glDisable(GL_TEXTURE_2D);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(btn.x, buttonY);
        glVertex2f(btn.x + btn.w, buttonY);
        glVertex2f(btn.x + btn.w, buttonY + buttonHeight);
        glVertex2f(btn.x, buttonY + buttonHeight);
        glEnd();

        // Button border
        glColor3f(0.7f, 0.7f, 0.7f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(btn.x, buttonY);
        glVertex2f(btn.x + btn.w, buttonY);
        glVertex2f(btn.x + btn.w, buttonY + buttonHeight);
        glVertex2f(btn.x, buttonY + buttonHeight);
        glEnd();

        // Draw label text using glyphs
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.2f, 0.2f, 0.2f, 1.0f);

        // Calculate text width for centering
        float textWidth = 0;
        for (const char* p = btn.label; *p; p++) {
            if (glyphs.find(*p) != glyphs.end()) {
                textWidth += glyphs[*p].advance;
            }
        }

        float textX = btn.x + (btn.w - textWidth) / 2.0f;
        float textY = buttonY + buttonHeight / 2.0f + 6.0f;  // Approximate vertical center

        for (const char* p = btn.label; *p; p++) {
            if (glyphs.find(*p) != glyphs.end()) {
                Glyph& g = glyphs[*p];
                float xpos = textX + g.bearingX;
                float ypos = textY - g.bearingY;
                float w = g.width;
                float h = g.height;

                glBindTexture(GL_TEXTURE_2D, g.textureID);
                glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos, ypos + h);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + w, ypos + h);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + w, ypos);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos, ypos);
                glEnd();

                textX += g.advance;
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

bool Engine::handleToolbarClick(double x, double y) {
    if (y >= TOOLBAR_HEIGHT) return false;  // Not in toolbar

    float buttonHeight = 26.0f;
    float buttonY = (TOOLBAR_HEIGHT - buttonHeight) / 2.0f;
    float startX = 10.0f;
    float spacing = 8.0f;

    // Button widths matching renderToolbar
    float widths[] = {30, 24, 36, 36, 36, 50};

    float currentX = startX;
    for (int i = 0; i < 6; i++) {
        float btnX = currentX;
        float btnW = widths[i];

        if (x >= btnX && x <= btnX + btnW &&
            y >= buttonY && y <= buttonY + buttonHeight) {
            // Button clicked
            switch (i) {
                case 0: applyBold(); break;
                case 1: applyItalic(); break;
                case 2: applyHeading(1); break;
                case 3: applyHeading(2); break;
                case 4: applyHeading(3); break;
                case 5: applyLink(); break;
            }
            return true;
        }

        currentX += btnW + spacing;
    }

    return false;
}

void Engine::wrapSelection(const std::string& before, const std::string& after) {
    if (!hasSelection) return;

    saveUndoState();

    int start = std::min(selectionStart, selectionEnd);
    int end = std::max(selectionStart, selectionEnd);
    int selLen = end - start;
    int insertLen = before.length() + after.length();

    if (inputLength + insertLen >= INPUT_BUFFER_SIZE - 1) return;

    // Make room for after text at end of selection
    memmove(inputBuffer + end + after.length(), inputBuffer + end, inputLength - end + 1);
    memcpy(inputBuffer + end, after.c_str(), after.length());
    inputLength += after.length();

    // Make room for before text at start of selection
    memmove(inputBuffer + start + before.length(), inputBuffer + start, inputLength - start + 1);
    memcpy(inputBuffer + start, before.c_str(), before.length());
    inputLength += before.length();

    // Update selection to cover wrapped text
    selectionStart = start;
    selectionEnd = start + before.length() + selLen + after.length();
    cursorPos = selectionEnd;

    // Update markdown content
    if (textBuffer && markdownRenderer) {
        std::string newText(inputBuffer, inputLength);
        textBuffer->setText(newText);
        markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
    }
}

void Engine::applyBold() {
    if (hasSelection) {
        wrapSelection("**", "**");
    } else {
        // Insert ** markers and place cursor between them
        saveUndoState();
        if (inputLength + 4 < INPUT_BUFFER_SIZE - 1) {
            memmove(inputBuffer + cursorPos + 4, inputBuffer + cursorPos, inputLength - cursorPos + 1);
            memcpy(inputBuffer + cursorPos, "****", 4);
            inputLength += 4;
            cursorPos += 2;  // Place cursor between ** markers

            if (textBuffer && markdownRenderer) {
                std::string newText(inputBuffer, inputLength);
                textBuffer->setText(newText);
                markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
            }
        }
    }
}

void Engine::applyItalic() {
    if (hasSelection) {
        wrapSelection("*", "*");
    } else {
        // Insert * markers and place cursor between them
        saveUndoState();
        if (inputLength + 2 < INPUT_BUFFER_SIZE - 1) {
            memmove(inputBuffer + cursorPos + 2, inputBuffer + cursorPos, inputLength - cursorPos + 1);
            memcpy(inputBuffer + cursorPos, "**", 2);
            inputLength += 2;
            cursorPos += 1;  // Place cursor between * markers

            if (textBuffer && markdownRenderer) {
                std::string newText(inputBuffer, inputLength);
                textBuffer->setText(newText);
                markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
            }
        }
    }
}

void Engine::applyHeading(int level) {
    saveUndoState();

    // Find start of current line - use selection start if there's a selection
    int targetPos = hasSelection ? std::min(selectionStart, selectionEnd) : cursorPos;
    int lineStart = findLineStart(targetPos);

    // Build heading prefix
    std::string prefix(level, '#');
    prefix += " ";

    // Check if line already has heading markers - remove them first
    int existingLevel = 0;
    int pos = lineStart;
    while (pos < inputLength && inputBuffer[pos] == '#') {
        existingLevel++;
        pos++;
    }
    if (existingLevel > 0 && pos < inputLength && inputBuffer[pos] == ' ') {
        pos++;  // Include space after #
    }

    if (existingLevel > 0) {
        // Remove existing heading prefix
        int removeLen = pos - lineStart;
        memmove(inputBuffer + lineStart, inputBuffer + pos, inputLength - pos + 1);
        inputLength -= removeLen;
        cursorPos -= removeLen;
        if (cursorPos < lineStart) cursorPos = lineStart;
    }

    // Insert new heading prefix at line start
    if (inputLength + prefix.length() < INPUT_BUFFER_SIZE - 1) {
        memmove(inputBuffer + lineStart + prefix.length(), inputBuffer + lineStart, inputLength - lineStart + 1);
        memcpy(inputBuffer + lineStart, prefix.c_str(), prefix.length());
        inputLength += prefix.length();
        cursorPos += prefix.length();

        if (textBuffer && markdownRenderer) {
            std::string newText(inputBuffer, inputLength);
            textBuffer->setText(newText);
            markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
        }
    }

    hasSelection = false;
}

void Engine::applyLink() {
    if (hasSelection) {
        // Wrap selected text as link text
        int start = std::min(selectionStart, selectionEnd);
        int end = std::max(selectionStart, selectionEnd);
        std::string selectedText(inputBuffer + start, end - start);

        saveUndoState();

        // Build link syntax: [selected text](url)
        std::string before = "[";
        std::string after = "](url)";

        if (inputLength + before.length() + after.length() >= INPUT_BUFFER_SIZE - 1) return;

        // Insert after part
        memmove(inputBuffer + end + after.length(), inputBuffer + end, inputLength - end + 1);
        memcpy(inputBuffer + end, after.c_str(), after.length());
        inputLength += after.length();

        // Insert before part
        memmove(inputBuffer + start + before.length(), inputBuffer + start, inputLength - start + 1);
        memcpy(inputBuffer + start, before.c_str(), before.length());
        inputLength += before.length();

        // Position cursor to select "url" placeholder
        int urlStart = start + before.length() + (end - start) + 2;  // After ](
        selectionStart = urlStart;
        selectionEnd = urlStart + 3;  // Select "url"
        cursorPos = selectionEnd;
        hasSelection = true;

        if (textBuffer && markdownRenderer) {
            std::string newText(inputBuffer, inputLength);
            textBuffer->setText(newText);
            markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
        }
    } else {
        // Insert link template
        saveUndoState();
        std::string linkTemplate = "[text](url)";
        if (inputLength + linkTemplate.length() < INPUT_BUFFER_SIZE - 1) {
            memmove(inputBuffer + cursorPos + linkTemplate.length(), inputBuffer + cursorPos, inputLength - cursorPos + 1);
            memcpy(inputBuffer + cursorPos, linkTemplate.c_str(), linkTemplate.length());
            inputLength += linkTemplate.length();

            // Select "text" placeholder
            selectionStart = cursorPos + 1;
            selectionEnd = cursorPos + 5;  // "text" is 4 chars, end is exclusive
            cursorPos = selectionEnd;
            hasSelection = true;

            if (textBuffer && markdownRenderer) {
                std::string newText(inputBuffer, inputLength);
                textBuffer->setText(newText);
                markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
            }
        }
    }
}

