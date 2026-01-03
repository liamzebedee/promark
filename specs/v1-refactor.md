- strong one-way flow of data
- boundary separation


Architecture:
    Render pipeline:
    - RawTextBuffer
    - MarkdownParser -> MarkdownObjectTree
    - MarkdownObjectTree -> computeLayoutTree -> LayoutTree
        - Inline, Block elements
        - Geometry
    - Painter -> Paint(MarkdownLayoutTree) -> DisplayItem[]
    - Rasterizer -> Raster(displayList) -> OpenGL calls

    Layout/geometry:
    - Hit tests (clicks, selections)
    - Raw mode vs. display mode (for swapping between them)

    Markdown elements
    - non-semantic elements
        - empty lines between blocks -> nothing in type mode
    - <br>
    - ``` code
    - tables
    - links
    - math equations
    - lists and sublists (alphabetical, numeric, dotpoint)
    - utf-8, emojii support

    Components:
    - UTF8 parsing
    - Typography - heading sizes, margins, padding
    - Image parsing
    - Fonts - regular, bold/italic, fixed-width vs. sans-serif
        - Embedding (system)
        - Glyph cache
    - Debug - layout object border rendering
    
    Styling:
    - Link highlighting
    - Frontmatter
    - Raw mode
    - Cursor animation
    - Drag and drop - images.
    - Window resizing, word wrapping

    Editor pipeline:
    - Engine
        - Caret/Cursor
        - Mouse
            - Position cursor
            - Single, double, triple click handling
        - Keyboard
            - Copy, paste
            - Select
            - Select to start/end of line
            - Select to start/end of doc
            - Undo buffer, undo operation debouncing (typing many letters quickly is one op)
        - Clipboard
        - Selection/highlight
        - Scrollbar
        - Toolbar
    
    