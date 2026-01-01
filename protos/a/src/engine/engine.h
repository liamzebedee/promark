#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <memory>
#include <vector>
#include "markdown_renderer.h"
#include "clipboard.h"
#include "batch_renderer.h"
#include "glyph_atlas.h"

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
    bool isOverLink(double x, double y);  // Check if mouse is over a clickable link

    // File operations
    void setContent(const std::string& content);
    std::string getContent() const;
    bool isDirty() const { return dirty; }
    void markClean() { dirty = false; }
    bool shouldClose() const { return wantsToClose; }

private:
    bool wantsToClose;
    bool leftMouseHeld;
    bool dirty;
    double lastClickTime;
    double lastClickX;
    double lastClickY;
    int clickCount;  // 1=single, 2=double, 3=triple
    float scrollOffset;
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
    FT_Face monoFace;
    bool fontLoaded;

    bool initFreeType();
    bool loadFont(const char* fontPath);
    
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

    // URL handling
    void openUrl(const std::string& url);

    // Undo system
    std::vector<UndoState> undoStack;
    static const int MAX_UNDO = 100;
    void saveUndoState();
    void undo();

    // Toolbar
    static const int TOOLBAR_HEIGHT = 40;
    void renderToolbar(int width);
    bool handleToolbarClick(double x, double y);

    // GL2 batch renderer for UI
    std::unique_ptr<BatchRenderer> uiRenderer;
    std::unique_ptr<GlyphAtlas> uiAtlas;
    bool uiRendererInitialized;
    void applyBold();
    void applyItalic();
    void applyHeading(int level);
    void applyLink();
    void wrapSelection(const std::string& before, const std::string& after);

    // Cursor animation
    float caretAnimX;
    float caretAnimY;
    float caretTargetX;
    float caretTargetY;
    double lastBlinkTime;
    bool caretVisible;
    void updateCaretAnimation();
};