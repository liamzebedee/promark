#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <map>
#include <memory>
#include <vector>
#include "markdown_renderer.h"
#include "clipboard.h"

struct UndoState {
    std::string text;
    int cursorPos;
};

class Engine {
public:
    Engine();
    ~Engine();
    
    bool initialize();
    void render(int width, int height);
    void handleKeyboard(int key, int scancode, int action, int mods);
    void handleScroll(double xoffset, double yoffset);
    void handleMouse(int button, int action, int mods, double x, double y);
    void handleMouseMove(double x, double y);

    // File operations
    void setContent(const std::string& content);
    std::string getContent() const;
    bool isDirty() const { return dirty; }
    void markClean() { dirty = false; }

private:
    bool leftMouseHeld;
    bool dirty;
    float scrollOffset;
    float scrollVelocity;
    float contentHeight;
    int viewportHeight;
    char* inputBuffer;
    static const int INPUT_BUFFER_SIZE = 10 * 1024 * 1024;  // 10MB
    int inputLength;
    
    // Text editing state
    int cursorPos;
    int goalColumn;  // Remembered column for vertical navigation
    int selectionStart;
    int selectionEnd;
    bool hasSelection;
    
    // Markdown rendering system
    std::unique_ptr<MarkdownRenderer> markdownRenderer;
    std::unique_ptr<TextBuffer> textBuffer;
    
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
    void renderCursor(float x, float y);
    void renderSelection(const char* text, float x, float y);
    bool initFreeType();
    bool loadFont(const char* fontPath);
    void loadGlyph(char c);
    
    // Text navigation helpers
    void moveCursor(int delta, bool extendSelection);
    void moveCursorByWord(int direction, bool extendSelection);
    void moveCursorVertically(int direction, bool extendSelection);
    int findWordBoundary(int pos, int direction);
    int findLineStart(int pos);
    int findLineEnd(int pos);
    int getColumnInLine(int pos);
    int findPositionInLine(int lineStart, int column);
    void insertChar(char c);
    void deleteChar();
    void deleteWordBackward();

    // Clipboard operations
    void selectAll();
    void copySelection();
    void paste();

    // Scroll helpers
    void ensureCursorVisible();

    // Undo system
    std::vector<UndoState> undoStack;
    static const int MAX_UNDO = 100;
    void saveUndoState();
    void undo();

    // Cursor animation
    float caretAnimX;
    float caretAnimY;
    float caretVelX;
    float caretVelY;
    float caretTargetX;
    float caretTargetY;
    double lastBlinkTime;
    bool caretVisible;
    void updateCaretAnimation();
};