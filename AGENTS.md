# Project Instructions

## Build & Run

```bash
cd protos/a
make        # Build (auto-fetches dependencies on first run)
./build/mdeditor  # Run the editor
```

## Clean

```bash
make clean      # Remove build artifacts
make clean-all  # Remove build + vendor dependencies
```

## Architecture

This is a markdown editor built with:
- **GLFW** - Window management and input handling
- **OpenGL** - Rendering
- **FreeType** - Font loading
- **HarfBuzz** - Text shaping
- **libjpeg-turbo** - Image decoding

### Key Components

| Component | Location | Purpose |
|-----------|----------|---------|
| Engine | `src/engine/engine.cpp` | Main editor logic, input handling |
| Clipboard | `src/engine/clipboard.cpp` | Platform-agnostic clipboard API |
| Markdown Parser | `src/engine/markdown_parser.cpp` | Parse markdown to DOM |
| Layout Engine | `src/engine/layout_engine.cpp` | Layout DOM to render tree |
| Painter | `src/engine/painter.cpp` | Paint operations (caret, selection) |
| Rasterizer | `src/engine/rasterizer.cpp` | OpenGL rendering |

### Input Handling

Keyboard and mouse input flows through callbacks in `main.cpp` to `Engine::handleKeyboard()` and `Engine::handleMouse()`. The design is platform-agnostic:
- Modifier detection uses both `GLFW_MOD_CONTROL` and `GLFW_MOD_SUPER` for cross-platform shortcuts
- Clipboard uses an abstract interface that can be swapped for web implementations

## Guidelines

- Do not take shortcuts with implementation unless instructed
- Prefer editing existing files over creating new ones
- Keep the architecture platform-agnostic where possible (desktop/web)
