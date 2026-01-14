#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <memory>
#include <vector>
#include "markdown_renderer.h"
#include "clipboard.h"
#include "opengl_backend.h"
#include "freetype_font_provider.h"

// Operation-based undo/redo system
// Stores the actual operation performed, enabling efficient undo/redo
// without storing full document copies
enum class TextOpType { Insert, Delete, Replace };

struct TextOperation {
    TextOpType type;
    size_t position;
    std::string insertedText;  // Text that was inserted (Insert/Replace)
    std::string deletedText;   // Text that was deleted (Delete/Replace)
};

struct UndoEntry {
    TextOperation operation;
    int caretPositionBefore;
    float scrollPositionBefore;
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
    std::string getSelectedText() const;
    bool isDirty() const { return textBuffer ? textBuffer->isDirty() : false; }
    void markClean() { if (textBuffer) textBuffer->markClean(); }
    bool shouldClose() const { return wantsToClose; }

    // Text insertion (for drag-and-drop, etc.)
    void insertText(const std::string& text);

private:
    bool wantsToClose;
    bool leftMouseHeld;
    double lastClickTime;
    double lastClickX;
    double lastClickY;
    int clickCount;  // 1=single, 2=double, 3=triple
    float scrollOffset;
    float contentHeight;
    int viewportHeight;

    // TextBuffer is the single source of truth for document content
    // All text operations go through this class
    std::unique_ptr<TextBuffer> textBuffer;
    
    // Text editing state
    int cursorPos;
    int goalColumn;  // Remembered column for vertical navigation
    int selectionStart;
    int selectionEnd;
    bool hasSelection;
    
    // Markdown rendering system
    std::unique_ptr<MarkdownRenderer> markdownRenderer;

    // FreeType font system
    FT_Library ft;
    FT_Face face;
    FT_Face monoFace;
    bool fontLoaded;

    // Font provider for layout layer (abstracts FT_Face)
    std::unique_ptr<FreeTypeFontProvider> fontProvider;

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

    // Operation-based undo/redo system
    std::vector<UndoEntry> undoStack;
    std::vector<UndoEntry> redoStack;
    static const int MAX_UNDO = 100;
    void recordInsert(size_t position, const std::string& text);
    void recordDelete(size_t position, const std::string& deletedText);
    void recordReplace(size_t position, const std::string& deletedText, const std::string& insertedText);
    void undo();
    void redo();

    // Toolbar
    static const int TOOLBAR_HEIGHT = 40;
    int viewportWidth;
    void renderToolbar(int width);
    bool handleToolbarClick(double x, double y);

    // Render backend - single authority for all GL calls
    // This backend is shared with MarkdownRenderer's Rasterizer
    std::unique_ptr<OpenGLBackend> renderBackend;
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

    // Raw mode toggle
    bool showRaw;
    void renderRawText(int width, int height);
    int hitTestRaw(float x, float y);
    float getCursorYRaw();
};