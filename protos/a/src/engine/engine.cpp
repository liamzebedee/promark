#include "engine.h"
#include "typography.h"
#include "gl_includes.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

Engine::Engine() : wantsToClose(false), leftMouseHeld(false), dirty(false), lastClickTime(0), lastClickX(0), lastClickY(0), clickCount(0),
                   scrollOffset(0.0f), contentHeight(0.0f), viewportHeight(0), inputBuffer(nullptr), inputLength(0),
                   fontLoaded(false), cursorPos(0), goalColumn(0), selectionStart(0), selectionEnd(0), hasSelection(false),
                   viewportWidth(800), uiRendererInitialized(false),
                   caretAnimX(0), caretAnimY(0),
                   caretTargetX(0), caretTargetY(0), lastBlinkTime(0), caretVisible(true),
                   showRaw(false) {
    // Allocate 10MB input buffer
    inputBuffer = new char[INPUT_BUFFER_SIZE];
    memset(inputBuffer, 0, INPUT_BUFFER_SIZE);
    inputLength = 0;
    cursorPos = 0;

    // Initialize markdown rendering system
    markdownRenderer = std::make_unique<MarkdownRenderer>();
    textBuffer = std::make_unique<TextBuffer>();
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
}

bool Engine::initialize() {
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifndef __EMSCRIPTEN__
    glEnable(GL_TEXTURE_2D);  // Not needed/valid in ES 2.0
#endif

    // Initialize FreeType
    if (!initFreeType()) {
        std::cerr << "Failed to initialize FreeType" << std::endl;
        return false;
    }

#ifdef __EMSCRIPTEN__
    const char* fontPaths[] = { "/fonts/NotoSans-Regular.ttf" };
#elif defined(__APPLE__)
    const char* fontPaths[] = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Arial.ttf",
        "/Library/Fonts/Arial.ttf"
    };
#else
    // Linux: bundled fonts
    const char* fontPaths[] = { "fonts/NotoSans-Regular.ttf" };
#endif

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

#ifdef __EMSCRIPTEN__
    const char* monoFontPaths[] = { "/fonts/NotoSansMono-Regular.ttf" };
#elif defined(__APPLE__)
    const char* monoFontPaths[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.dfont",
        "/Library/Fonts/Courier New.ttf"
    };
#else
    const char* monoFontPaths[] = { "fonts/NotoSansMono-Regular.ttf" };
#endif

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
    viewportWidth = width;
    viewportHeight = height - TOOLBAR_HEIGHT;
    int contentAreaHeight = height - TOOLBAR_HEIGHT;

    // Clamp scroll to valid range (with bottom margin for breathing room)
    float bottomMargin = 60.0f;
    float maxScroll = std::max(0.0f, contentHeight - contentAreaHeight + bottomMargin);
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    glViewport(0, 0, width, height);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render toolbar at top (fixed position, no scroll)
    renderToolbar(width);

    // Set up clipping for content area (below toolbar)
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, width, height - TOOLBAR_HEIGHT);

    // Update cursor blink (320ms cycle)
    double currentTime = glfwGetTime();
    if (currentTime - lastBlinkTime > 0.320) {
        caretVisible = !caretVisible;
        lastBlinkTime = currentTime;
    }

    if (showRaw) {
        // Render raw text mode
        renderRawText(width, height);
    } else if (markdownRenderer) {
        int domCursorPos = markdownRenderer->rawToDOM(cursorPos);
        int domSelStart = markdownRenderer->rawToDOM(selectionStart);
        int domSelEnd = markdownRenderer->rawToDOM(selectionEnd);

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
        // Pass scroll offset: TOOLBAR_HEIGHT shifts content below toolbar,
        // -scrollOffset shifts content up for scrolling
        float contentScrollOffset = TOOLBAR_HEIGHT - scrollOffset;
        markdownRenderer->render(viewportSize, contentScrollOffset);

        contentHeight = markdownRenderer->getContentHeight();

        // Update animated cursor position (lerp toward target)
        updateCaretAnimation();
    }

    // Disable scissor test before drawing fixed UI elements
    glDisable(GL_SCISSOR_TEST);

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

        // Thumb position
        float scrollRatio = (maxScroll > 0) ? scrollOffset / maxScroll : 0;
        float thumbY;
        if (scrollRatio < 0) {
            thumbY = trackTop;
        } else if (scrollRatio > 1) {
            thumbY = trackTop + trackHeight - thumbHeight;
        } else {
            thumbY = trackTop + scrollRatio * (trackHeight - thumbHeight);
        }

        // Draw scrollbar thumb using batch renderer (ES 2.0 compatible)
        uiRenderer->setViewport(width, height);
        uiRenderer->begin();
        uiRenderer->drawRect(trackX, thumbY, scrollbarWidth, thumbHeight, 0.5f, 0.5f, 0.5f, 0.5f);
        uiRenderer->flush();
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
            } else if (key == GLFW_KEY_R) {
                // Get cursor's current screen Y position before toggle
                float cursorScreenY = 0;
                if (showRaw) {
                    cursorScreenY = getCursorYRaw() - scrollOffset;
                } else if (markdownRenderer) {
                    int domPos = markdownRenderer->rawToDOM(cursorPos);
                    cursorScreenY = markdownRenderer->getCursorY(domPos) - scrollOffset;
                }

                showRaw = !showRaw;

                // Get cursor's new content Y position after toggle
                float newCursorY = 0;
                if (showRaw) {
                    newCursorY = getCursorYRaw();
                } else if (markdownRenderer) {
                    int domPos = markdownRenderer->rawToDOM(cursorPos);
                    newCursorY = markdownRenderer->getCursorY(domPos);
                }

                // Adjust scroll to keep cursor at same screen position
                scrollOffset = newCursorY - cursorScreenY;
                if (scrollOffset < 0) scrollOffset = 0;
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
    // Direct scroll - multiply by line height for reasonable speed
    scrollOffset += -yoffset * 40.0f;
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

            if (showRaw) {
                // Raw mode: simple monospace hit test directly on raw buffer
                cursorPos = hitTestRaw(static_cast<float>(x), static_cast<float>(y));
                cursorPos = std::max(0, std::min(cursorPos, inputLength));
            } else if (markdownRenderer) {
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

                cursorPos = markdownRenderer->hitTest(static_cast<float>(x), contentY);
                cursorPos = std::max(0, std::min(cursorPos, inputLength));
            }

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

            // Snap caret animation to new position immediately (don't animate on click)
            if (markdownRenderer) {
                int domPos = markdownRenderer->rawToDOM(cursorPos);
                markdownRenderer->getCursorXY(domPos, caretTargetX, caretTargetY);
                caretAnimX = caretTargetX;
                caretAnimY = caretTargetY;
            }

            lastClickTime = currentTime;
            lastClickX = x;
            lastClickY = y;
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
    if (leftMouseHeld) {
        int newPos;
        if (showRaw) {
            newPos = hitTestRaw(static_cast<float>(x), static_cast<float>(y));
        } else if (markdownRenderer) {
            // Adjust y for toolbar and add scroll offset
            float contentY = static_cast<float>(y - TOOLBAR_HEIGHT) + scrollOffset;
            newPos = markdownRenderer->hitTest(static_cast<float>(x), contentY);
        } else {
            return;
        }
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
        return false;
    }
    fontLoaded = true;
    return true;
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

void Engine::insertText(const std::string& text) {
    if (text.empty()) return;

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

    int insertLen = static_cast<int>(text.length());
    if (inputLength + insertLen < INPUT_BUFFER_SIZE - 1) {
        memmove(inputBuffer + cursorPos + insertLen, inputBuffer + cursorPos, inputLength - cursorPos + 1);
        memcpy(inputBuffer + cursorPos, text.c_str(), insertLen);
        inputLength += insertLen;
        cursorPos += insertLen;

        // Update markdown content
        if (textBuffer && markdownRenderer) {
            std::string newText(inputBuffer, inputLength);
            textBuffer->setText(newText);
            markdownRenderer->setTextBuffer(std::make_unique<TextBuffer>(*textBuffer));
        }
        ensureCursorVisible();
        dirty = true;

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

std::string Engine::getSelectedText() const {
    if (!hasSelection) return "";
    int start = std::min(selectionStart, selectionEnd);
    int end = std::max(selectionStart, selectionEnd);
    return std::string(inputBuffer + start, end - start);
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

float Engine::getCursorYRaw() {
    // Use same layout constants as renderRawText
    float fontSize = Typography::BASE_FONT_SIZE;
    float lineHeight = fontSize * 1.2f;
    float leftMargin = Typography::DOCUMENT_MARGIN;
    float rightMargin = Typography::DOCUMENT_MARGIN;
    float maxLineWidth = viewportWidth - leftMargin - rightMargin;
    float topMargin = Typography::DOCUMENT_MARGIN;  // Content space, no toolbar offset

    // Calculate character width for monospace
    float charWidth = fontSize * 0.6f;
    if (monoFace && uiAtlas) {
        FT_Set_Pixel_Sizes(monoFace, 0, static_cast<FT_UInt>(fontSize));
        const AtlasGlyph* g = uiAtlas->get('M', fontSize, TextStyle::Normal, true, monoFace);
        if (g) charWidth = g->advance;
    }

    float lineY = topMargin + fontSize;
    float lineX = 0;
    int lineStart = 0;

    for (int i = 0; i < inputLength; i++) {
        // Check if cursor is at this position
        if (i == cursorPos) {
            return lineY;
        }

        char c = inputBuffer[i];

        if (c == '\n') {
            lineY += lineHeight;
            lineX = 0;
            lineStart = i + 1;
            continue;
        }

        // Word wrap check
        if (lineX + charWidth > maxLineWidth && i > lineStart) {
            int breakPoint = -1;
            for (int j = i - 1; j >= lineStart; j--) {
                if (inputBuffer[j] == ' ') {
                    breakPoint = j;
                    break;
                }
            }

            if (breakPoint > lineStart) {
                lineY += lineHeight;
                lineStart = breakPoint + 1;
                lineX = (i - lineStart) * charWidth;
            } else {
                lineY += lineHeight;
                lineStart = i;
                lineX = 0;
            }
        }

        lineX += charWidth;
    }

    // Cursor at end
    return lineY;
}

int Engine::hitTestRaw(float x, float y) {
    // Use same layout constants as renderRawText
    float fontSize = Typography::BASE_FONT_SIZE;
    float lineHeight = fontSize * 1.2f;
    float leftMargin = Typography::DOCUMENT_MARGIN;
    float rightMargin = Typography::DOCUMENT_MARGIN;
    float maxLineWidth = viewportWidth - leftMargin - rightMargin;
    float topMargin = TOOLBAR_HEIGHT + Typography::DOCUMENT_MARGIN - scrollOffset;

    // Calculate character width for monospace
    float charWidth = fontSize * 0.6f;
    if (monoFace && uiAtlas) {
        FT_Set_Pixel_Sizes(monoFace, 0, static_cast<FT_UInt>(fontSize));
        const AtlasGlyph* g = uiAtlas->get('M', fontSize, TextStyle::Normal, true, monoFace);
        if (g) charWidth = g->advance;
    }

    // Build line structure matching renderRawText
    struct RawLine {
        int startIdx;
        int endIdx;
        float yPos;
    };
    std::vector<RawLine> lines;

    float lineY = topMargin + fontSize;
    float lineX = 0;
    int lineStart = 0;

    for (int i = 0; i < inputLength; i++) {
        char c = inputBuffer[i];

        if (c == '\n') {
            lines.push_back({lineStart, i, lineY});
            lineY += lineHeight;
            lineX = 0;
            lineStart = i + 1;
            continue;
        }

        // Word wrap check
        if (lineX + charWidth > maxLineWidth && i > lineStart) {
            // Find break point (last space)
            int breakPoint = -1;
            for (int j = i - 1; j >= lineStart; j--) {
                if (inputBuffer[j] == ' ') {
                    breakPoint = j;
                    break;
                }
            }

            if (breakPoint > lineStart) {
                lines.push_back({lineStart, breakPoint, lineY});
                lineY += lineHeight;
                lineStart = breakPoint + 1;
                lineX = (i - lineStart) * charWidth;
            } else {
                lines.push_back({lineStart, i, lineY});
                lineY += lineHeight;
                lineStart = i;
                lineX = 0;
            }
        }

        lineX += charWidth;
    }

    // Add final line
    lines.push_back({lineStart, inputLength, lineY});

    // Find which line was clicked
    int clickedLineIdx = -1;
    for (size_t i = 0; i < lines.size(); i++) {
        float lineTop = lines[i].yPos - fontSize;
        float lineBottom = lines[i].yPos + (lineHeight - fontSize);
        if (y >= lineTop && y < lineBottom) {
            clickedLineIdx = static_cast<int>(i);
            break;
        }
    }

    // If no line found, find closest
    if (clickedLineIdx < 0) {
        if (y < topMargin) {
            clickedLineIdx = 0;
        } else {
            clickedLineIdx = static_cast<int>(lines.size()) - 1;
        }
    }

    if (lines.empty()) return 0;

    const RawLine& line = lines[clickedLineIdx];
    int lineLen = line.endIdx - line.startIdx;

    // Find character position based on x
    float relX = x - leftMargin;
    if (relX < 0) {
        return line.startIdx;
    }

    int charIdx = static_cast<int>(relX / charWidth);
    // Use half-character threshold for more accurate positioning
    float remainder = relX - (charIdx * charWidth);
    if (remainder > charWidth / 2) {
        charIdx++;
    }

    if (charIdx > lineLen) {
        charIdx = lineLen;
    }

    return line.startIdx + charIdx;
}

void Engine::renderRawText(int width, int height) {
    // Initialize UI renderer if needed
    if (!uiRendererInitialized) {
        uiAtlas = std::make_unique<GlyphAtlas>(512, 512);
        uiRenderer = std::make_unique<BatchRenderer>();
        uiAtlas->init();
        uiRenderer->init();
        uiRenderer->setAtlas(uiAtlas.get());
        uiRendererInitialized = true;
    }

    float fontSize = Typography::BASE_FONT_SIZE;
    float lineHeight = fontSize * 1.2f;
    float leftMargin = Typography::DOCUMENT_MARGIN;
    float rightMargin = Typography::DOCUMENT_MARGIN;
    float maxLineWidth = width - leftMargin - rightMargin;
    float topMargin = TOOLBAR_HEIGHT + Typography::DOCUMENT_MARGIN - scrollOffset;

    // Calculate character width for monospace
    float charWidth = fontSize * 0.6f;
    if (monoFace) {
        FT_Set_Pixel_Sizes(monoFace, 0, static_cast<FT_UInt>(fontSize));
        const AtlasGlyph* g = uiAtlas->get('M', fontSize, TextStyle::Normal, true, monoFace);
        if (g) charWidth = g->advance;
    }

    // Build wrapped lines with syntax highlighting info
    struct RenderChar {
        char c;
        float r, g, b;  // Color
        int srcIndex;   // Original index in inputBuffer
    };
    struct RenderLine {
        std::vector<RenderChar> chars;
        float y;
    };
    std::vector<RenderLine> lines;

    // Colors for syntax highlighting
    auto colorDefault = [](float& r, float& g, float& b) { r = 0.2f; g = 0.2f; b = 0.2f; };
    auto colorHeading = [](float& r, float& g, float& b) { r = 0.0f; g = 0.4f; b = 0.8f; };
    auto colorCode = [](float& r, float& g, float& b) { r = 0.7f; g = 0.2f; b = 0.2f; };
    auto colorLink = [](float& r, float& g, float& b) { r = 0.1f; g = 0.5f; b = 0.1f; };
    auto colorBoldItalic = [](float& r, float& g, float& b) { r = 0.6f; g = 0.3f; b = 0.6f; };
    auto colorList = [](float& r, float& g, float& b) { r = 0.8f; g = 0.5f; b = 0.0f; };
    auto colorBlockquote = [](float& r, float& g, float& b) { r = 0.4f; g = 0.6f; b = 0.4f; };

    // Parse and wrap text
    float y = topMargin + fontSize;
    float x = 0;
    RenderLine currentLine;
    currentLine.y = y;

    bool inCodeBlock = false;
    bool inInlineCode = false;
    bool lineStart = true;
    bool isHeadingLine = false;
    bool isListLine = false;
    bool isBlockquoteLine = false;

    for (int i = 0; i < inputLength; i++) {
        char c = inputBuffer[i];

        // Detect line-level syntax at start of line
        if (lineStart && c != '\n') {
            // Check for code block fence
            if (i + 2 < inputLength && inputBuffer[i] == '`' && inputBuffer[i+1] == '`' && inputBuffer[i+2] == '`') {
                inCodeBlock = !inCodeBlock;
            }
            // Check for heading
            if (!inCodeBlock && c == '#') {
                isHeadingLine = true;
            }
            // Check for list item
            if (!inCodeBlock && (c == '-' || c == '*' || c == '+' || (c >= '0' && c <= '9'))) {
                isListLine = true;
            }
            // Check for blockquote
            if (!inCodeBlock && c == '>') {
                isBlockquoteLine = true;
            }
            lineStart = false;
        }

        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.chars.clear();
            y += lineHeight;
            currentLine.y = y;
            x = 0;
            lineStart = true;
            isHeadingLine = false;
            isListLine = false;
            isBlockquoteLine = false;
            continue;
        }

        // Check for inline code toggle
        if (!inCodeBlock && c == '`') {
            inInlineCode = !inInlineCode;
        }

        // Word wrap: if adding this char exceeds width, start new line
        if (x + charWidth > maxLineWidth && !currentLine.chars.empty()) {
            // Try to break at last space
            int breakPoint = -1;
            for (int j = (int)currentLine.chars.size() - 1; j >= 0; j--) {
                if (currentLine.chars[j].c == ' ') {
                    breakPoint = j;
                    break;
                }
            }

            if (breakPoint > 0) {
                // Break at space
                RenderLine wrappedLine;
                wrappedLine.y = currentLine.y;
                for (int j = 0; j <= breakPoint; j++) {
                    wrappedLine.chars.push_back(currentLine.chars[j]);
                }
                lines.push_back(wrappedLine);

                // Move remaining chars to new line
                std::vector<RenderChar> remaining;
                for (int j = breakPoint + 1; j < (int)currentLine.chars.size(); j++) {
                    remaining.push_back(currentLine.chars[j]);
                }
                y += lineHeight;
                currentLine.chars = remaining;
                currentLine.y = y;
                x = remaining.size() * charWidth;
            } else {
                // No space found, hard break
                lines.push_back(currentLine);
                currentLine.chars.clear();
                y += lineHeight;
                currentLine.y = y;
                x = 0;
            }
        }

        // Determine color
        RenderChar rc;
        rc.c = c;
        rc.srcIndex = i;

        if (inCodeBlock || inInlineCode) {
            colorCode(rc.r, rc.g, rc.b);
        } else if (isHeadingLine) {
            colorHeading(rc.r, rc.g, rc.b);
        } else if (isBlockquoteLine) {
            colorBlockquote(rc.r, rc.g, rc.b);
        } else if (isListLine && currentLine.chars.size() < 3) {
            colorList(rc.r, rc.g, rc.b);
        } else if (c == '*' || c == '_') {
            colorBoldItalic(rc.r, rc.g, rc.b);
        } else if (c == '[' || c == ']' || c == '(' || c == ')') {
            colorLink(rc.r, rc.g, rc.b);
        } else {
            colorDefault(rc.r, rc.g, rc.b);
        }

        currentLine.chars.push_back(rc);
        x += charWidth;
    }
    // Add final line
    if (!currentLine.chars.empty() || lines.empty()) {
        lines.push_back(currentLine);
    }

    uiRenderer->setViewport(width, height);
    uiRenderer->begin();

    // Draw selection background
    int selStart = std::min(selectionStart, selectionEnd);
    int selEnd = std::max(selectionStart, selectionEnd);
    if (hasSelection && selStart != selEnd) {
        for (const auto& line : lines) {
            for (size_t j = 0; j < line.chars.size(); j++) {
                int idx = line.chars[j].srcIndex;
                if (idx >= selStart && idx < selEnd) {
                    float selX = leftMargin + j * charWidth;
                    uiRenderer->drawRect(selX, line.y - fontSize, charWidth, lineHeight, 0.68f, 0.84f, 1.0f, 0.7f);
                }
            }
        }
    }

    // Render characters
    for (const auto& line : lines) {
        float lx = leftMargin;
        for (const auto& rc : line.chars) {
            if (monoFace) {
                char str[2] = {rc.c, '\0'};
                uiRenderer->drawText(str, lx, line.y, rc.r, rc.g, rc.b, 1.0f, fontSize, TextStyle::Normal, true, monoFace);
            }
            lx += charWidth;
        }
    }

    // Draw cursor
    if (caretVisible && !hasSelection) {
        float cursorX = leftMargin;
        float cursorY = topMargin + fontSize;
        for (const auto& line : lines) {
            bool found = false;
            for (size_t j = 0; j < line.chars.size(); j++) {
                if (line.chars[j].srcIndex == cursorPos) {
                    cursorX = leftMargin + j * charWidth;
                    cursorY = line.y;
                    found = true;
                    break;
                }
            }
            if (found) break;
            // Check if cursor is at end of this line
            if (!line.chars.empty() && line.chars.back().srcIndex == cursorPos - 1) {
                cursorX = leftMargin + line.chars.size() * charWidth;
                cursorY = line.y;
            }
        }
        // Handle cursor at very end
        if (cursorPos == inputLength && !lines.empty()) {
            const auto& lastLine = lines.back();
            cursorX = leftMargin + lastLine.chars.size() * charWidth;
            cursorY = lastLine.y;
        }
        uiRenderer->drawRect(cursorX, cursorY - fontSize, 2.0f, lineHeight, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    uiRenderer->flush();

    // Content height for scrolling
    contentHeight = (lines.size() + 1) * lineHeight + 40.0f;
}

void Engine::renderToolbar(int width) {
    // Initialize UI renderer on first use
    if (!uiRendererInitialized) {
        uiAtlas = std::make_unique<GlyphAtlas>(512, 512);
        uiRenderer = std::make_unique<BatchRenderer>();
        uiAtlas->init();
        uiRenderer->init();
        uiRenderer->setAtlas(uiAtlas.get());
        uiRendererInitialized = true;
    }

    uiRenderer->setViewport(width, viewportHeight > 0 ? viewportHeight : 600);
    uiRenderer->begin();

    // Draw toolbar background (light gray)
    uiRenderer->drawRect(0, 0, width, TOOLBAR_HEIGHT, 0.95f, 0.95f, 0.95f, 1.0f);

    // Draw bottom border
    uiRenderer->drawRect(0, TOOLBAR_HEIGHT - 1, width, 1, 0.8f, 0.8f, 0.8f, 1.0f);

    // Button definitions
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
        {startX, 30, "B"},
        {0, 24, "I"},
        {0, 36, "H1"},
        {0, 36, "H2"},
        {0, 36, "H3"},
        {0, 50, "Link"}
    };

    // Calculate positions
    float currentX = startX;
    for (int i = 0; i < 6; i++) {
        buttons[i].x = currentX;
        currentX += buttons[i].w + spacing;
    }

    // Draw buttons
    for (int i = 0; i < 6; i++) {
        Button& btn = buttons[i];

        // Button background (white)
        uiRenderer->drawRect(btn.x, buttonY, btn.w, buttonHeight, 1.0f, 1.0f, 1.0f, 1.0f);

        // Button border (draw as 4 thin rects)
        float bw = 1.0f;  // border width
        uiRenderer->drawRect(btn.x, buttonY, btn.w, bw, 0.7f, 0.7f, 0.7f, 1.0f);  // top
        uiRenderer->drawRect(btn.x, buttonY + buttonHeight - bw, btn.w, bw, 0.7f, 0.7f, 0.7f, 1.0f);  // bottom
        uiRenderer->drawRect(btn.x, buttonY, bw, buttonHeight, 0.7f, 0.7f, 0.7f, 1.0f);  // left
        uiRenderer->drawRect(btn.x + btn.w - bw, buttonY, bw, buttonHeight, 0.7f, 0.7f, 0.7f, 1.0f);  // right

        // Draw label text
        float textWidth = 0;
        for (const char* p = btn.label; *p; p++) {
            const AtlasGlyph* g = uiAtlas->get(*p, 16, TextStyle::Normal, false, face);
            if (g) textWidth += g->advance;
        }

        float textX = btn.x + (btn.w - textWidth) / 2.0f;
        float textY = buttonY + buttonHeight / 2.0f + 6.0f;

        uiRenderer->drawText(btn.label, textX, textY, 0.2f, 0.2f, 0.2f, 1.0f, 16, TextStyle::Normal, false, face);
    }

    // Draw "Raw" toggle button on the right side
    float rawBtnWidth = 50.0f;
    float rawBtnX = width - rawBtnWidth - 10.0f;

    // Button background - highlighted if showRaw is active
    if (showRaw) {
        uiRenderer->drawRect(rawBtnX, buttonY, rawBtnWidth, buttonHeight, 0.85f, 0.9f, 1.0f, 1.0f);
    } else {
        uiRenderer->drawRect(rawBtnX, buttonY, rawBtnWidth, buttonHeight, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Button border
    float bw = 1.0f;
    uiRenderer->drawRect(rawBtnX, buttonY, rawBtnWidth, bw, 0.7f, 0.7f, 0.7f, 1.0f);  // top
    uiRenderer->drawRect(rawBtnX, buttonY + buttonHeight - bw, rawBtnWidth, bw, 0.7f, 0.7f, 0.7f, 1.0f);  // bottom
    uiRenderer->drawRect(rawBtnX, buttonY, bw, buttonHeight, 0.7f, 0.7f, 0.7f, 1.0f);  // left
    uiRenderer->drawRect(rawBtnX + rawBtnWidth - bw, buttonY, bw, buttonHeight, 0.7f, 0.7f, 0.7f, 1.0f);  // right

    // Draw "Raw" label
    const char* rawLabel = "Raw";
    float rawTextWidth = 0;
    for (const char* p = rawLabel; *p; p++) {
        const AtlasGlyph* g = uiAtlas->get(*p, 16, TextStyle::Normal, false, face);
        if (g) rawTextWidth += g->advance;
    }
    float rawTextX = rawBtnX + (rawBtnWidth - rawTextWidth) / 2.0f;
    float rawTextY = buttonY + buttonHeight / 2.0f + 6.0f;
    uiRenderer->drawText(rawLabel, rawTextX, rawTextY, 0.2f, 0.2f, 0.2f, 1.0f, 16, TextStyle::Normal, false, face);

    uiRenderer->flush();
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

    // Check for "Raw" button click (on the right side)
    float rawBtnWidth = 50.0f;
    float rawBtnX = viewportWidth - rawBtnWidth - 10.0f;
    if (x >= rawBtnX && x <= rawBtnX + rawBtnWidth &&
        y >= buttonY && y <= buttonY + buttonHeight) {
        showRaw = !showRaw;
        return true;
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

