#include "engine.h"
#include "typography.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

Engine::Engine() : wantsToClose(false), leftMouseHeld(false), lastClickTime(0), lastClickX(0), lastClickY(0), clickCount(0),
                   scrollOffset(0.0f), contentHeight(0.0f), viewportHeight(0),
                   fontLoaded(false), cursorPos(0), goalColumn(0), selectionStart(0), selectionEnd(0), hasSelection(false),
                   viewportWidth(800),
                   caretAnimX(0), caretAnimY(0),
                   caretTargetX(0), caretTargetY(0), lastBlinkTime(0), caretVisible(true),
                   showRaw(false) {
    // TextBuffer is the single source of truth for document content
    textBuffer = std::make_unique<TextBuffer>();
    cursorPos = 0;

    // Initialize markdown rendering system with pointer to our TextBuffer
    markdownRenderer = std::make_unique<MarkdownRenderer>();
    markdownRenderer->setTextBuffer(textBuffer.get());
}

Engine::~Engine() {
    if (fontLoaded) {
        FT_Done_Face(face);
        if (monoFace) {
            FT_Done_Face(monoFace);
        }
        FT_Done_FreeType(ft);
    }
}

bool Engine::initialize() {
    // Create and initialize the render backend (single authority for all GL calls)
    renderBackend = std::make_unique<OpenGLBackend>();
    if (!renderBackend->init()) {
        std::cerr << "Failed to initialize render backend" << std::endl;
        return false;
    }

    // Pass backend to markdown renderer's rasterizer
    markdownRenderer->setBackend(renderBackend.get());

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

    // Create font provider and pass to markdown renderer for layout/glyph metrics
    fontProvider = std::make_unique<FreeTypeFontProvider>(face, monoFace);
    markdownRenderer->setFontProvider(fontProvider.get());

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

    // Begin frame with the render backend
    renderBackend->beginFrame(width, height);
    renderBackend->setViewport(0, 0, width, height);
    renderBackend->clear(1.0f, 1.0f, 1.0f, 1.0f);

    // Render toolbar at top (fixed position, no scroll)
    renderToolbar(width);

    // Set up clipping for content area (below toolbar)
    renderBackend->pushClip(0, 0, width, height - TOOLBAR_HEIGHT);

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
        Size viewportSize(width, contentAreaHeight);

        // IMPORTANT: Ensure layout is valid BEFORE updating caret animation.
        // This allows getCursorXY() to return accurate positions from the fresh layout.
        markdownRenderer->ensureLayoutValid(viewportSize);

        // Update animated cursor position with accurate target from layout.
        // This must happen BEFORE setCaretState so the painted caret uses the correct position.
        updateCaretAnimation();

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

        // documentScrollY: actual scroll position in document space (for viewport culling)
        // gpuScrollOffset: TOOLBAR_HEIGHT - scrollOffset (shifts content for toolbar and scroll)
        float gpuScrollOffset = TOOLBAR_HEIGHT - scrollOffset;
        markdownRenderer->render(viewportSize, scrollOffset, gpuScrollOffset);

        contentHeight = markdownRenderer->getContentHeight();
    }

    // Pop clip before drawing fixed UI elements
    renderBackend->popClip();

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

        // Draw scrollbar thumb using render backend
        renderBackend->setScrollOffset(0);  // No scroll for fixed UI
        renderBackend->drawRect(trackX, thumbY, scrollbarWidth, thumbHeight, 0.5f, 0.5f, 0.5f, 0.5f);
        renderBackend->flush();
    }

    renderBackend->endFrame();
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
                if (shift) {
                    redo();  // Ctrl/Cmd+Shift+Z = Redo
                } else {
                    undo();  // Ctrl/Cmd+Z = Undo
                }
                return;
            } else if (key == GLFW_KEY_Y) {
                redo();  // Ctrl/Cmd+Y = Redo (alternative shortcut)
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
                cursorPos = static_cast<int>(textBuffer->getLength());
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
                int start = std::min(selectionStart, selectionEnd);
                int end = std::max(selectionStart, selectionEnd);
                std::string deletedText = textBuffer->getText().substr(start, end - start);
                recordDelete(start, deletedText);
                textBuffer->deleteText(start, end - start);
                cursorPos = start;
                goalColumn = getColumnInLine(cursorPos);
                hasSelection = false;
            } else if (cmdOrCtrl && cursorPos > 0) {
                // Cmd/Ctrl+Backspace: delete to start of line
                int lineStart = findLineStart(cursorPos);
                if (lineStart < cursorPos) {
                    // Delete from cursor to start of line
                    int deleteCount = cursorPos - lineStart;
                    std::string deletedText = textBuffer->getText().substr(lineStart, deleteCount);
                    recordDelete(lineStart, deletedText);
                    textBuffer->deleteText(lineStart, deleteCount);
                    cursorPos = lineStart;
                } else if (cursorPos > 0) {
                    // Already at start of line - delete the newline to merge with previous line
                    std::string deletedText(1, textBuffer->charAt(cursorPos - 1));
                    recordDelete(cursorPos - 1, deletedText);
                    textBuffer->deleteText(cursorPos - 1, 1);
                    cursorPos--;
                }
                goalColumn = getColumnInLine(cursorPos);
            } else if (alt && cursorPos > 0) {
                // Alt+Backspace: delete word backwards
                deleteWordBackward();
            } else if (cursorPos > 0) {
                deleteChar();
            }
            
        } else if (key == GLFW_KEY_DELETE) {
            int inputLength = static_cast<int>(textBuffer->getLength());
            if (hasSelection) {
                // Delete selection
                int start = std::min(selectionStart, selectionEnd);
                int end = std::max(selectionStart, selectionEnd);
                std::string deletedText = textBuffer->getText().substr(start, end - start);
                recordDelete(start, deletedText);
                textBuffer->deleteText(start, end - start);
                cursorPos = start;
                hasSelection = false;
            } else if (cursorPos < inputLength) {
                std::string deletedText(1, textBuffer->charAt(cursorPos));
                recordDelete(cursorPos, deletedText);
                textBuffer->deleteText(cursorPos, 1);
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
            cursorPos = static_cast<int>(textBuffer->getLength());
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

            int inputLength = static_cast<int>(textBuffer->getLength());
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
                if (lineEnd < inputLength && textBuffer->charAt(lineEnd) == '\n') {
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
        int inputLength = static_cast<int>(textBuffer->getLength());
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
    int inputLength = static_cast<int>(textBuffer->getLength());
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
    int inputLength = static_cast<int>(textBuffer->getLength());
    if (direction > 0) {
        // Move forward to next word
        while (pos < inputLength && textBuffer->charAt(pos) != ' ' && textBuffer->charAt(pos) != '\n') pos++;
        while (pos < inputLength && (textBuffer->charAt(pos) == ' ' || textBuffer->charAt(pos) == '\n')) pos++;
    } else {
        // Move backward to previous word
        while (pos > 0 && (textBuffer->charAt(pos-1) == ' ' || textBuffer->charAt(pos-1) == '\n')) pos--;
        while (pos > 0 && textBuffer->charAt(pos-1) != ' ' && textBuffer->charAt(pos-1) != '\n') pos--;
    }
    return pos;
}

void Engine::insertChar(char c) {
    std::string s(1, c);

    if (hasSelection) {
        // Replace selection
        int start = std::min(selectionStart, selectionEnd);
        int end = std::max(selectionStart, selectionEnd);
        std::string deletedText = textBuffer->getText().substr(start, end - start);
        recordReplace(start, deletedText, s);
        textBuffer->replaceText(start, end - start, s);
        cursorPos = start + 1;
        hasSelection = false;
    } else {
        recordInsert(cursorPos, s);
        textBuffer->insertText(cursorPos, s);
        cursorPos++;
    }

    ensureCursorVisible();

    // Reset blink timer so cursor is visible after typing
    lastBlinkTime = glfwGetTime();
    caretVisible = true;
    goalColumn = getColumnInLine(cursorPos);
}

void Engine::insertText(const std::string& text) {
    if (text.empty()) return;

    if (hasSelection) {
        // Replace selection
        int start = std::min(selectionStart, selectionEnd);
        int end = std::max(selectionStart, selectionEnd);
        std::string deletedText = textBuffer->getText().substr(start, end - start);
        recordReplace(start, deletedText, text);
        textBuffer->replaceText(start, end - start, text);
        cursorPos = start + static_cast<int>(text.length());
        hasSelection = false;
    } else {
        recordInsert(cursorPos, text);
        textBuffer->insertText(cursorPos, text);
        cursorPos += static_cast<int>(text.length());
    }

    ensureCursorVisible();

    // Reset blink timer so cursor is visible after typing
    lastBlinkTime = glfwGetTime();
    caretVisible = true;
    goalColumn = getColumnInLine(cursorPos);
}

void Engine::deleteChar() {
    if (cursorPos > 0) {
        std::string deletedText(1, textBuffer->charAt(cursorPos - 1));
        recordDelete(cursorPos - 1, deletedText);
        textBuffer->deleteText(cursorPos - 1, 1);
        cursorPos--;
        goalColumn = getColumnInLine(cursorPos);
    }
}

void Engine::deleteWordBackward() {
    if (cursorPos > 0) {
        int wordStart = findWordBoundary(cursorPos, -1);
        int deleteCount = cursorPos - wordStart;

        if (deleteCount > 0) {
            std::string deletedText = textBuffer->getText().substr(wordStart, deleteCount);
            recordDelete(wordStart, deletedText);
            textBuffer->deleteText(wordStart, deleteCount);
            cursorPos = wordStart;
            goalColumn = getColumnInLine(cursorPos);
        }
    }
}

int Engine::findLineStart(int pos) {
    while (pos > 0 && textBuffer->charAt(pos - 1) != '\n') {
        pos--;
    }
    return pos;
}

int Engine::findLineEnd(int pos) {
    int inputLength = static_cast<int>(textBuffer->getLength());
    while (pos < inputLength && textBuffer->charAt(pos) != '\n') {
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
    int inputLength = static_cast<int>(textBuffer->getLength());
    int newPos = cursorPos;

    // Use visual line navigation in markdown mode (not raw mode)
    if (!showRaw && markdownRenderer) {
        // Ensure layout is valid
        Size viewportSize(viewportWidth, viewportHeight);
        markdownRenderer->ensureLayoutValid(viewportSize);

        // Get current cursor's visual position
        int domPos = markdownRenderer->rawToDOM(cursorPos);
        float cursorX, cursorY;
        markdownRenderer->getCursorXY(domPos, cursorX, cursorY);

        // Use current cursor X for hit testing (preserves horizontal position during vertical nav)
        float targetX = cursorX;

        // Calculate the visual line spacing for navigation
        // In markdown mode, paragraphs are separated by block spacing, and each paragraph
        // has its own height. The actual visual spacing between line tops includes:
        // - The paragraph's content height (font size * number of lines)
        // - Block spacing between paragraphs
        // For simple single-line paragraphs, this is typically font_size + block_spacing,
        // but empirically the actual spacing is closer to 2*font_size.
        float lineHeight = Typography::BASE_FONT_SIZE;
        // Use a larger step to ensure we land in the next line's hit region
        // The hit region for each line has a 0.2*fontSize offset
        float lineSpacing = lineHeight * 2;  // Conservative step that works for standard paragraphs

        // Calculate target Y position on adjacent visual line
        // getCursorXY returns the top of the current line
        // The hit region for each line is offset by 0.2*fontSize from the rect top
        // To ensure we land inside the next line's hit region, aim for the vertical center
        // of the target line (add half lineHeight to the base step)
        float targetY = cursorY + (direction * lineSpacing) + (lineHeight * 0.5f);

        // Use hit testing to find position at target coordinates
        // hitTest returns raw position
        int hitPos = markdownRenderer->hitTest(targetX, targetY);

        // DEBUG: Print navigation info
        std::cerr << "DEBUG moveCursorVertically: dir=" << direction
                  << " cursorPos=" << cursorPos
                  << " domPos=" << domPos
                  << " cursorXY=(" << cursorX << "," << cursorY << ")"
                  << " targetXY=(" << targetX << "," << targetY << ")"
                  << " hitPos=" << hitPos << std::endl;

        // Only use hitPos if it actually moved us (avoid staying in place)
        if (hitPos != cursorPos) {
            newPos = hitPos;
        } else {
            // If hit test didn't move us, we're at a document boundary
            // newPos stays as cursorPos
        }
    } else {
        // Raw mode: use logical line navigation (original behavior)
        int currentLineStart = findLineStart(cursorPos);

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
    int inputLength = static_cast<int>(textBuffer->getLength());
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
    std::string selectedText = textBuffer->getText().substr(start, end - start);
    Clipboard::setText(selectedText);
}

void Engine::paste() {
    std::string clipboardText = Clipboard::getText();
    if (clipboardText.empty()) {
        return;
    }

    if (hasSelection) {
        // Replace selection with clipboard text
        int start = std::min(selectionStart, selectionEnd);
        int end = std::max(selectionStart, selectionEnd);
        std::string deletedText = textBuffer->getText().substr(start, end - start);
        recordReplace(start, deletedText, clipboardText);
        textBuffer->replaceText(start, end - start, clipboardText);
        cursorPos = start + static_cast<int>(clipboardText.length());
        hasSelection = false;
    } else {
        // Insert clipboard text at cursor
        recordInsert(cursorPos, clipboardText);
        textBuffer->insertText(cursorPos, clipboardText);
        cursorPos += static_cast<int>(clipboardText.length());
    }

    ensureCursorVisible();
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

    // Calculate distance to target
    float dx = caretTargetX - caretAnimX;
    float dy = caretTargetY - caretAnimY;
    float distance = std::sqrt(dx * dx + dy * dy);

    // If target moved significantly (e.g., cursor jumped to new position),
    // snap immediately instead of lerping. This prevents the "lagging caret" bug
    // when typing or pressing Enter.
    const float snapThreshold = 20.0f;  // pixels
    if (distance > snapThreshold) {
        caretAnimX = caretTargetX;
        caretAnimY = caretTargetY;
        return;
    }

    // Simple smooth lerp for small movements (e.g., single character)
    float t = 0.4f;
    caretAnimX += dx * t;
    caretAnimY += dy * t;

    // Snap when very close
    if (std::abs(dx) < 0.5f) caretAnimX = caretTargetX;
    if (std::abs(dy) < 0.5f) caretAnimY = caretTargetY;
}

void Engine::recordInsert(size_t position, const std::string& text) {
    UndoEntry entry;
    entry.operation.type = TextOpType::Insert;
    entry.operation.position = position;
    entry.operation.insertedText = text;
    entry.caretPositionBefore = cursorPos;
    entry.scrollPositionBefore = scrollOffset;

    undoStack.push_back(entry);
    redoStack.clear();  // Clear redo stack on new operation

    if (undoStack.size() > MAX_UNDO) {
        undoStack.erase(undoStack.begin());
    }
}

void Engine::recordDelete(size_t position, const std::string& deletedText) {
    UndoEntry entry;
    entry.operation.type = TextOpType::Delete;
    entry.operation.position = position;
    entry.operation.deletedText = deletedText;
    entry.caretPositionBefore = cursorPos;
    entry.scrollPositionBefore = scrollOffset;

    undoStack.push_back(entry);
    redoStack.clear();

    if (undoStack.size() > MAX_UNDO) {
        undoStack.erase(undoStack.begin());
    }
}

void Engine::recordReplace(size_t position, const std::string& deletedText, const std::string& insertedText) {
    UndoEntry entry;
    entry.operation.type = TextOpType::Replace;
    entry.operation.position = position;
    entry.operation.deletedText = deletedText;
    entry.operation.insertedText = insertedText;
    entry.caretPositionBefore = cursorPos;
    entry.scrollPositionBefore = scrollOffset;

    undoStack.push_back(entry);
    redoStack.clear();

    if (undoStack.size() > MAX_UNDO) {
        undoStack.erase(undoStack.begin());
    }
}

void Engine::setContent(const std::string& content) {
    textBuffer->setText(content);
    textBuffer->markClean();  // Content just loaded, not dirty
    cursorPos = 0;
    goalColumn = 0;
    hasSelection = false;
    undoStack.clear();
    redoStack.clear();
}

std::string Engine::getContent() const {
    return textBuffer->getText();
}

std::string Engine::getSelectedText() const {
    if (!hasSelection) return "";
    int start = std::min(selectionStart, selectionEnd);
    int end = std::max(selectionStart, selectionEnd);
    return textBuffer->getText().substr(start, end - start);
}

void Engine::undo() {
    if (undoStack.empty()) return;

    UndoEntry entry = undoStack.back();
    undoStack.pop_back();

    // Create redo entry (captures current state before undo)
    UndoEntry redoEntry;
    redoEntry.operation = entry.operation;
    redoEntry.caretPositionBefore = cursorPos;
    redoEntry.scrollPositionBefore = scrollOffset;
    redoStack.push_back(redoEntry);

    // Invert the operation
    const TextOperation& op = entry.operation;
    switch (op.type) {
        case TextOpType::Insert:
            // Undo insert = delete the inserted text
            textBuffer->deleteText(op.position, op.insertedText.length());
            break;
        case TextOpType::Delete:
            // Undo delete = insert the deleted text back
            textBuffer->insertText(op.position, op.deletedText);
            break;
        case TextOpType::Replace:
            // Undo replace = replace inserted text with deleted text
            textBuffer->replaceText(op.position, op.insertedText.length(), op.deletedText);
            break;
    }

    // Restore caret and scroll position
    int inputLength = static_cast<int>(textBuffer->getLength());
    cursorPos = std::min(entry.caretPositionBefore, inputLength);
    scrollOffset = entry.scrollPositionBefore;
    goalColumn = getColumnInLine(cursorPos);
    hasSelection = false;

    // Reset blink
    lastBlinkTime = glfwGetTime();
    caretVisible = true;
}

void Engine::redo() {
    if (redoStack.empty()) return;

    UndoEntry entry = redoStack.back();
    redoStack.pop_back();

    // Create undo entry (captures current state before redo)
    UndoEntry undoEntry;
    undoEntry.operation = entry.operation;
    undoEntry.caretPositionBefore = cursorPos;
    undoEntry.scrollPositionBefore = scrollOffset;
    undoStack.push_back(undoEntry);

    // Re-apply the operation
    const TextOperation& op = entry.operation;
    switch (op.type) {
        case TextOpType::Insert:
            // Redo insert = insert the text again
            textBuffer->insertText(op.position, op.insertedText);
            cursorPos = static_cast<int>(op.position + op.insertedText.length());
            break;
        case TextOpType::Delete:
            // Redo delete = delete the text again
            textBuffer->deleteText(op.position, op.deletedText.length());
            cursorPos = static_cast<int>(op.position);
            break;
        case TextOpType::Replace:
            // Redo replace = replace deleted text with inserted text
            textBuffer->replaceText(op.position, op.deletedText.length(), op.insertedText);
            cursorPos = static_cast<int>(op.position + op.insertedText.length());
            break;
    }

    goalColumn = getColumnInLine(cursorPos);
    hasSelection = false;

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

    // Calculate character width for monospace using fontProvider
    float charWidth = fontProvider->getGlyphAdvance('M', static_cast<int>(fontSize), true);

    float lineY = topMargin + fontSize;
    float lineX = 0;
    int lineStart = 0;
    int inputLength = static_cast<int>(textBuffer->getLength());

    for (int i = 0; i < inputLength; i++) {
        // Check if cursor is at this position
        if (i == cursorPos) {
            return lineY;
        }

        char c = textBuffer->charAt(i);

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
                if (textBuffer->charAt(j) == ' ') {
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

    // Calculate character width for monospace using fontProvider
    float charWidth = fontProvider->getGlyphAdvance('M', static_cast<int>(fontSize), true);

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
    int inputLength = static_cast<int>(textBuffer->getLength());

    for (int i = 0; i < inputLength; i++) {
        char c = textBuffer->charAt(i);

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
                if (textBuffer->charAt(j) == ' ') {
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
    (void)height;  // Unused after removing uiRenderer

    float fontSize = Typography::BASE_FONT_SIZE;
    float lineHeight = fontSize * 1.2f;
    float leftMargin = Typography::DOCUMENT_MARGIN;
    float rightMargin = Typography::DOCUMENT_MARGIN;
    float maxLineWidth = width - leftMargin - rightMargin;
    float topMargin = TOOLBAR_HEIGHT + Typography::DOCUMENT_MARGIN - scrollOffset;

    // Calculate character width for monospace using fontProvider
    float charWidth = fontProvider->getGlyphAdvance('M', static_cast<int>(fontSize), true);

    // Build wrapped lines with syntax highlighting info
    struct RenderChar {
        char c;
        float r, g, b;  // Color
        int srcIndex;   // Original index in textBuffer
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
    bool lineStartFlag = true;
    bool isHeadingLine = false;
    bool isListLine = false;
    bool isBlockquoteLine = false;
    int inputLength = static_cast<int>(textBuffer->getLength());

    for (int i = 0; i < inputLength; i++) {
        char c = textBuffer->charAt(i);

        // Detect line-level syntax at start of line
        if (lineStartFlag && c != '\n') {
            // Check for code block fence
            if (i + 2 < inputLength && textBuffer->charAt(i) == '`' && textBuffer->charAt(i+1) == '`' && textBuffer->charAt(i+2) == '`') {
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
            lineStartFlag = false;
        }

        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.chars.clear();
            y += lineHeight;
            currentLine.y = y;
            x = 0;
            lineStartFlag = true;
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

    // Set scroll offset for raw text rendering
    renderBackend->setScrollOffset(0);  // Top margin already includes scroll offset

    // Draw selection background
    int selStart = std::min(selectionStart, selectionEnd);
    int selEnd = std::max(selectionStart, selectionEnd);
    if (hasSelection && selStart != selEnd) {
        for (const auto& line : lines) {
            for (size_t j = 0; j < line.chars.size(); j++) {
                int idx = line.chars[j].srcIndex;
                if (idx >= selStart && idx < selEnd) {
                    float selX = leftMargin + j * charWidth;
                    renderBackend->drawRect(selX, line.y - fontSize, charWidth, lineHeight, 0.68f, 0.84f, 1.0f, 0.7f);
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
                renderBackend->drawText(str, lx, line.y, rc.r, rc.g, rc.b, 1.0f, static_cast<int>(fontSize), TextStyle::Normal, true, monoFace);
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
        renderBackend->drawRect(cursorX, cursorY - fontSize, 2.0f, lineHeight, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    renderBackend->flush();

    // Content height for scrolling
    contentHeight = (lines.size() + 1) * lineHeight + 40.0f;
}

void Engine::renderToolbar(int width) {
    // Use render backend for all drawing (no scroll offset for fixed toolbar)
    renderBackend->setScrollOffset(0);

    // Draw toolbar background (light gray)
    renderBackend->drawRect(0, 0, width, TOOLBAR_HEIGHT, 0.95f, 0.95f, 0.95f, 1.0f);

    // Draw bottom border
    renderBackend->drawRect(0, TOOLBAR_HEIGHT - 1, width, 1, 0.8f, 0.8f, 0.8f, 1.0f);

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
        renderBackend->drawRect(btn.x, buttonY, btn.w, buttonHeight, 1.0f, 1.0f, 1.0f, 1.0f);

        // Button border (draw as 4 thin rects)
        float bw = 1.0f;  // border width
        renderBackend->drawRect(btn.x, buttonY, btn.w, bw, 0.7f, 0.7f, 0.7f, 1.0f);  // top
        renderBackend->drawRect(btn.x, buttonY + buttonHeight - bw, btn.w, bw, 0.7f, 0.7f, 0.7f, 1.0f);  // bottom
        renderBackend->drawRect(btn.x, buttonY, bw, buttonHeight, 0.7f, 0.7f, 0.7f, 1.0f);  // left
        renderBackend->drawRect(btn.x + btn.w - bw, buttonY, bw, buttonHeight, 0.7f, 0.7f, 0.7f, 1.0f);  // right

        // Draw label text - use fontProvider for width calculation
        float textWidth = 0;
        for (const char* p = btn.label; *p; p++) {
            textWidth += fontProvider->getGlyphAdvance(*p, 16, false);
        }

        float textX = btn.x + (btn.w - textWidth) / 2.0f;
        float textY = buttonY + buttonHeight / 2.0f + 6.0f;

        renderBackend->drawText(btn.label, textX, textY, 0.2f, 0.2f, 0.2f, 1.0f, 16, TextStyle::Normal, false, face);
    }

    // Draw "Raw" toggle button on the right side
    float rawBtnWidth = 50.0f;
    float rawBtnX = width - rawBtnWidth - 10.0f;

    // Button background - highlighted if showRaw is active
    if (showRaw) {
        renderBackend->drawRect(rawBtnX, buttonY, rawBtnWidth, buttonHeight, 0.85f, 0.9f, 1.0f, 1.0f);
    } else {
        renderBackend->drawRect(rawBtnX, buttonY, rawBtnWidth, buttonHeight, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Button border
    float bw = 1.0f;
    renderBackend->drawRect(rawBtnX, buttonY, rawBtnWidth, bw, 0.7f, 0.7f, 0.7f, 1.0f);  // top
    renderBackend->drawRect(rawBtnX, buttonY + buttonHeight - bw, rawBtnWidth, bw, 0.7f, 0.7f, 0.7f, 1.0f);  // bottom
    renderBackend->drawRect(rawBtnX, buttonY, bw, buttonHeight, 0.7f, 0.7f, 0.7f, 1.0f);  // left
    renderBackend->drawRect(rawBtnX + rawBtnWidth - bw, buttonY, bw, buttonHeight, 0.7f, 0.7f, 0.7f, 1.0f);  // right

    // Draw "Raw" label
    const char* rawLabel = "Raw";
    float rawTextWidth = 0;
    for (const char* p = rawLabel; *p; p++) {
        rawTextWidth += fontProvider->getGlyphAdvance(*p, 16, false);
    }
    float rawTextX = rawBtnX + (rawBtnWidth - rawTextWidth) / 2.0f;
    float rawTextY = buttonY + buttonHeight / 2.0f + 6.0f;
    renderBackend->drawText(rawLabel, rawTextX, rawTextY, 0.2f, 0.2f, 0.2f, 1.0f, 16, TextStyle::Normal, false, face);

    renderBackend->flush();
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

    int start = std::min(selectionStart, selectionEnd);
    int end = std::max(selectionStart, selectionEnd);
    int selLen = end - start;

    // Get selected text and wrap it
    std::string selectedText = textBuffer->getText().substr(start, selLen);
    std::string wrapped = before + selectedText + after;

    // Record the replace operation
    recordReplace(start, selectedText, wrapped);

    // Replace selection with wrapped text
    textBuffer->replaceText(start, selLen, wrapped);

    // Update selection to cover wrapped text
    selectionStart = start;
    selectionEnd = start + static_cast<int>(wrapped.length());
    cursorPos = selectionEnd;
}

void Engine::applyBold() {
    if (hasSelection) {
        wrapSelection("**", "**");
    } else {
        // Insert ** markers and place cursor between them
        recordInsert(cursorPos, "****");
        textBuffer->insertText(cursorPos, "****");
        cursorPos += 2;  // Place cursor between ** markers
    }
}

void Engine::applyItalic() {
    if (hasSelection) {
        wrapSelection("*", "*");
    } else {
        // Insert * markers and place cursor between them
        recordInsert(cursorPos, "**");
        textBuffer->insertText(cursorPos, "**");
        cursorPos += 1;  // Place cursor between * markers
    }
}

void Engine::applyHeading(int level) {
    // Find start of current line - use selection start if there's a selection
    int targetPos = hasSelection ? std::min(selectionStart, selectionEnd) : cursorPos;
    int lineStart = findLineStart(targetPos);

    // Build heading prefix
    std::string prefix(level, '#');
    prefix += " ";

    // Check if line already has heading markers
    int inputLength = static_cast<int>(textBuffer->getLength());
    int existingLevel = 0;
    int pos = lineStart;
    while (pos < inputLength && textBuffer->charAt(pos) == '#') {
        existingLevel++;
        pos++;
    }
    if (existingLevel > 0 && pos < inputLength && textBuffer->charAt(pos) == ' ') {
        pos++;  // Include space after #
    }

    // Get the old prefix to replace
    std::string oldPrefix = textBuffer->getText().substr(lineStart, pos - lineStart);

    // Record as a single Replace operation
    if (!oldPrefix.empty() || !prefix.empty()) {
        recordReplace(lineStart, oldPrefix, prefix);
    }

    // Perform the actual replacement
    if (!oldPrefix.empty()) {
        textBuffer->deleteText(lineStart, oldPrefix.length());
        cursorPos -= static_cast<int>(oldPrefix.length());
        if (cursorPos < lineStart) cursorPos = lineStart;
    }

    textBuffer->insertText(lineStart, prefix);
    cursorPos += static_cast<int>(prefix.length());

    hasSelection = false;
}

void Engine::applyLink() {
    if (hasSelection) {
        // Wrap selected text as link text
        int start = std::min(selectionStart, selectionEnd);
        int end = std::max(selectionStart, selectionEnd);
        int selLen = end - start;
        std::string selectedText = textBuffer->getText().substr(start, selLen);

        // Build link syntax: [selected text](url)
        std::string wrapped = "[" + selectedText + "](url)";
        recordReplace(start, selectedText, wrapped);
        textBuffer->replaceText(start, selLen, wrapped);

        // Position cursor to select "url" placeholder
        int urlStart = start + 1 + selLen + 2;  // After [selectedText](
        selectionStart = urlStart;
        selectionEnd = urlStart + 3;  // Select "url"
        cursorPos = selectionEnd;
        hasSelection = true;
    } else {
        // Insert link template
        std::string linkTemplate = "[text](url)";
        recordInsert(cursorPos, linkTemplate);
        textBuffer->insertText(cursorPos, linkTemplate);

        // Select "text" placeholder
        selectionStart = cursorPos + 1;
        selectionEnd = cursorPos + 5;  // "text" is 4 chars, end is exclusive
        cursorPos = selectionEnd;
        hasSelection = true;
    }
}

