# Web Build

Build the markdown editor for web browsers using Emscripten.

## Setup

Install Emscripten SDK (one time):

```bash
make setup
```

This clones emsdk to `../vendor/emsdk` and installs the latest version.

## Build

```bash
source ../vendor/emsdk/emsdk_env.sh
make
```

Output goes to `build/` directory.

## Run

```bash
make serve
```

Opens http://localhost:8000

## JavaScript API

```javascript
// Set editor content
MDEditor.setContent("# Hello\n\nWorld");

// Get editor content
const md = MDEditor.getContent();

// Check if content changed
if (MDEditor.isDirty()) { ... }

// Mark as saved
MDEditor.markClean();
```
