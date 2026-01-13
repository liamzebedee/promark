promark - a professional markdown editor
========================================

Promark is a professional markdown editor. It is the fastest Markdown editor ever - native app, C++ and OpenGL, no frameworks like electron, using OpenGL ES 2.0. 

- Native performance, typographic beauty
    - feels like using word, but you own your own data (.md) files
    - written using native tech, not web electron crap. feels good, low on battery
    - like sublime text for typographic markdown writing.

It is inspired by Chrome's web browser architecture. One-way flow of data. 

## V1 design (WIP).

How does it work? 
- Raw text buffer: raw markdown text
- Parser -> object tree. Objects: heading, image, formatting (bold, italic, underline, link), block quote, code block, equations, lists
- Layout/geometry: determine the visual geometry of all the elements in the object tree. 
    - There are two flows: block flow and inline flow.
    - Block flow places elements one after another, descending vertically.
    - Text and "inline" elements flow left-to-right, and are broken into lines.
    - Images require loading and computing their size. 
    - Layout measures runs of text in fonts. Shaping selects the glyphs and computes their placement.
- Object tree -> layout tree.
    - Layout operates on a separate tree, linked to the object tree
- Paint
    - Now that we understand the geometry of our layout objects, it's time to paint them.
    - Paint records paint operations into a list of display items.
    - A paint operation might be something like "draw a rectangle at these coordinates, in this color".
- Raster
    - Rasterization turns (part of) a display item list into a bitmap of color values.
    - Raster also decodes image resources embedded in the page.  The paint op references the compressed data (JPEG, PNG, etc), and raster invokes the appropriate decoder to decompress it.
- Rasterization issues OpenGL calls
- Render into the window.

Architecture:
    Window and UI:
    - caret animation
    - window resizing.
        - when window resize happens, we live resize the content
    - content wrapping 
        - ie. words follow to new lines. tables/images are scaled.
    - Drag and drop - images.
        - paths or base64 embedding?
    - link interactivity
    
    Editor views:
    - Raw mode and visual mode
    - Coordinate translation
        - Absolute (UI) -> document object (text, image, etc) -> source position (line, pos)
    - Scene graph / universal hit test translation

    Visual mode render engine:
    - raw text
    - markdown parser
    - document object tree
    - layout object tree
        - computed from (document object tree, viewport)
            - LayoutObject(position, box(w,h), children)
            - viewport important for wrapping content
        - computes:
            - geometry for Inline, Block elements
        - handles:
            - text
                - word wrapping for lines
            - image geometry <- parse images, render
            - font geometry
                - loads fonts (fixed, sans)
                - builds glyph cache
                - loads document text fragment, computes font size, font family (mono for code/frontmatter, sans for else), font variant (bold, italic)
                - renders all glyphs, compute layout object (a line of text)
            - complex nested layout geometry:
                - tables
                - lists
    - painter
        - records paint operations into a list of display items
        - e.g. "draw a rectangle at these coordinates, in this color"
        - organises itself as a PaintTree, with the node being PaintArtifact(displayItems[])
        - handles special case flows:
            - paint caret
            - paint highlight
            - debugging paints - debug borders for boxes
    - rasterize
        - turns (part of) a display item list into a bitmap of color values
        - issues opengl calls to render to a content surface buffer
    - viz
        - displays the content surface buffer on the window

    Raw mode render engine:
    - raw text
    - markdown parser
    - document object tree
    - layout object tree
        - everything is fixed-width, so all geometry is uniform
    - painter
    - rasterize
    - viz

    Markdown elements
    - basic
        - text - bold, italic
        - links
        - inline code
        - lists and sublists (alphabetical, numeric, dotpoint)
        - paragraphs
        - empty paragraphs
        - headings (h1..h6)
        - images
        - code (block, inline)
        - quotes (block, inline)
    - non-semantic elements
        - empty lines between blocks -> nothing in type mode
    - <br>
    - tables
    - math equations
    - utf-8, emojii support
    - frontmatter parsing (---). 
        - render it as a code block in plaintext. fixed-width. use a nice fixed-width font. use a gray background with black text. only a small amount of margin.

    Markdown save/load:
    - accept/read any type of markdown. discard empty lines and formatting.
    - output pretty formatted markdown

    Components:
    - UTF8 parsing
    - Image parsing
        - jpeg
        - png
    - Fonts
        - Families - fixed-width, sans-serif
        - Variants - regular, bold/italic
        - Locations and loading
            - Libre fonts
            - System fonts
            - Embedding/linking
        - Glyph cache

    Cross platform elements:
    - Keyboard handling
        - Command key (cmd, ctrl)

    Editor:
    - Caret/Cursor
        - Caret is a flashing indicator
        - Typing on the keyboard inserts characters at the position of the cursor, which re-renders. 
        - Backspace on the keyboard deletes text and images.
    - Selection/highlight
    - Mouse
        - Position cursor
        - Clicking within a line's "hit box" positions the cursor
        - Single, double, triple click handling
    - Keyboard
        - Copy, paste
        - Backspace
        - Select (arrow keys)
        - Select all (ctrl+A)
        - cmd+up/down for quick scroll up/down
        - cmd+left/right for select/delete left/right (works within a line)
            - Select to start/end of line
            - Select to start/end of doc
        - Undo buffer
            - undo should reposition caret where you are on the line
            - undo operation debouncing (typing many letters quickly is one op)
        - Insert soft newline (shift+enter)
        - Insert newline (enter) 
    - Clipboard
    - Scrollbar
    - Toolbar

    Visual mode styling:
    - Typography - heading sizes, margins, padding
    - Lists
    - Tables
    - Quote blocks have a thick gray left border (5px) and are indented from that.
    - Code has a background color is rendered using fixed-width font rendering.

    Software:
    - WebGL port
    - CLI version (mdedit <file>.md)


make it so there are two editing views: raw and visual

raw mode is editing raw markdown text. there are no images or blocks.
visual mode is editing wysiwig. you can see images, blocks, etc.

both modes share the same caret and highlight selections. when you swap modes (ctrl+r toggle), you are placed in the same position of each view into the document. the highlight is the same too.


## Future features.

Future things:
- OneNote-like layout
    - Spatial arrangement.
        - Notebooks
        - Sections
        - Pages
            - Subpages
        - Content
            - Blocks are positioned absolutely on infinite canvas
            - (x,y) per block and sizing
- Simple file naming / virtual notes index
    - I want to drag-and-drop media and make quick notes of EVERYTHING
    - Adding a file name is an impediment
    - Would be useful to just add titles (Notion) and that's it
- Render web content
    - Video
        - video decoding
        - frame compositing
        - drag and drop videos
- Notebooking
    - ability to screenclip anything you see 
    - add to notebook
    - including videos
- Edit my notebook on liamzebedee.com
    - Same view. Nice visual. With columns. Native editing. Add images to background.
- Background agents.
    - Journal
        - Just add stuff to my journal
        - Background agents constantly analyse and think about it
- Writing website
    - It would be good to have a website dedicated to imagination.
    - ie. like the youtube comment algorithm, it steers users towards exploration and imagineering
    - writing is all about making notes. "What if we reimplemented a browser engine?"
