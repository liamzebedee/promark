# Web build using Emscripten

# Emscripten SDK path (install with: git clone https://github.com/emscripten-core/emsdk.git vendor/emsdk)
EMSDK_DIR = ../vendor/emsdk
EMCC = $(EMSDK_DIR)/upstream/emscripten/emcc

# Shared source list (paths relative to parent dir)
include ../sources.mk
ENGINE_SRC_WEB = $(addprefix ../,$(ENGINE_SRC))

WEB_SRC = web.cpp

# Output
OUTPUT = build/mdeditor.js

# Compiler flags
CXXFLAGS = -std=c++17 -O2 \
           -I../src \
           -Wno-unused-parameter

# Emscripten ports (auto-downloads and builds)
PORTS = -sUSE_FREETYPE=1 \
        -sUSE_HARFBUZZ=1 \
        -sUSE_GLFW=3 \
        -sUSE_LIBJPEG=1

# WebGL/OpenGL ES 2.0 settings
GL_FLAGS = -sFULL_ES2=1 \
           -sGL_ENABLE_GET_PROC_ADDRESS=1

# Memory settings
MEM_FLAGS = -sALLOW_MEMORY_GROWTH=1 \
            -sINITIAL_MEMORY=67108864

# Export settings
EXPORT_FLAGS = -sEXPORTED_FUNCTIONS='["_main","_setContent","_getContent","_isDirty","_markClean","_malloc","_free"]' \
               -sEXPORTED_RUNTIME_METHODS='["UTF8ToString","stringToUTF8","lengthBytesUTF8"]'

# Embed fonts and shaders
EMBED_FLAGS = --embed-file fonts@/fonts --embed-file ../src/engine/shaders@/shaders

# All linker flags
LDFLAGS = $(PORTS) $(GL_FLAGS) $(MEM_FLAGS) $(EXPORT_FLAGS) $(EMBED_FLAGS)

.PHONY: all clean setup fonts serve

all: check-emsdk fonts build/mdeditor.js copy-html

check-emsdk:
	@if [ ! -f "$(EMCC)" ]; then \
		echo "Error: Emscripten not found at $(EMCC)"; \
		echo "Run 'make setup' to install Emscripten SDK"; \
		exit 1; \
	fi

setup:
	@echo "Installing Emscripten SDK..."
	@if [ ! -d "../vendor/emsdk" ]; then \
		git clone https://github.com/emscripten-core/emsdk.git ../vendor/emsdk; \
	fi
	cd ../vendor/emsdk && ./emsdk install latest && ./emsdk activate latest
	@echo ""
	@echo "Emscripten installed. Run: source ../vendor/emsdk/emsdk_env.sh"
	@echo "Then run: make"

fonts:
	@mkdir -p fonts
	@if [ ! -f "fonts/NotoSans-Regular.ttf" ]; then \
		echo "Downloading Noto Sans fonts..."; \
		curl -L -o fonts/NotoSans-Regular.ttf "https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf"; \
		curl -L -o fonts/NotoSans-Bold.ttf "https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Bold.ttf"; \
		curl -L -o fonts/NotoSans-Italic.ttf "https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Italic.ttf"; \
		curl -L -o fonts/NotoSans-BoldItalic.ttf "https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-BoldItalic.ttf"; \
	fi
	@if [ ! -f "fonts/NotoSansMono-Regular.ttf" ]; then \
		echo "Downloading Noto Sans Mono font..."; \
		curl -L -o fonts/NotoSansMono-Regular.ttf "https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSansMono/hinted/ttf/NotoSansMono-Regular.ttf"; \
	fi

build/mdeditor.js: $(WEB_SRC) $(ENGINE_SRC_WEB) | build
	$(EMCC) $(CXXFLAGS) $(WEB_SRC) $(ENGINE_SRC_WEB) -o $(OUTPUT) $(LDFLAGS)

build:
	mkdir -p build

copy-html:
	cp index.html build/

clean:
	rm -rf build

serve: all
	@echo "Starting server at http://localhost:8000"
	cd build && python3.13 -m http.server 8000
