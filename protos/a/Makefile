CXX = g++
VENDOR_DIR = vendor
GLFW_BUILD_DIR = $(VENDOR_DIR)/glfw-build
FREETYPE_BUILD_DIR = $(VENDOR_DIR)/freetype-build
HARFBUZZ_BUILD_DIR = $(VENDOR_DIR)/harfbuzz-build
LIBJPEG_BUILD_DIR = $(VENDOR_DIR)/libjpeg-turbo-build
SRC_DIR = src
BUILD_DIR = build

# Optimization: use 'make release' or 'make OPT=1'
ifdef OPT
OPTFLAGS = -O3 -DNDEBUG -flto
else
OPTFLAGS = -g
endif

CXXFLAGS = -std=c++17 -Wall -Wextra \
	$(OPTFLAGS) \
	-DGL_SILENCE_DEPRECATION \
	-Wno-unused-parameter \
	-Wno-reorder \
	-Wno-sign-compare \
	-I$(SRC_DIR) \
	-I$(GLFW_BUILD_DIR)/install/include \
	-I$(FREETYPE_BUILD_DIR)/install/include/freetype2 \
	-I$(HARFBUZZ_BUILD_DIR)/install/include/harfbuzz \
	-I$(LIBJPEG_BUILD_DIR)/install/include

LDFLAGS = -L$(GLFW_BUILD_DIR)/install/lib \
	-L$(FREETYPE_BUILD_DIR)/install/lib \
	-L$(HARFBUZZ_BUILD_DIR)/install/lib \
	-L$(LIBJPEG_BUILD_DIR)/install/lib \
	-lglfw3 -lfreetype -lharfbuzz -ljpeg \
	-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

TARGET = $(BUILD_DIR)/mdeditor
TARGET_EDIT = $(BUILD_DIR)/mdedit

ENGINE_SOURCES = $(SRC_DIR)/engine/engine.cpp \
	$(SRC_DIR)/engine/text_buffer.cpp \
	$(SRC_DIR)/engine/clipboard.cpp \
	$(SRC_DIR)/engine/markdown_objects.cpp \
	$(SRC_DIR)/engine/markdown_parser.cpp \
	$(SRC_DIR)/engine/layout_objects.cpp \
	$(SRC_DIR)/engine/layout_engine.cpp \
	$(SRC_DIR)/engine/paint_operations.cpp \
	$(SRC_DIR)/engine/painter.cpp \
	$(SRC_DIR)/engine/rasterizer.cpp \
	$(SRC_DIR)/engine/markdown_renderer.cpp \
	$(SRC_DIR)/engine/glyph_atlas.cpp \
	$(SRC_DIR)/engine/batch_renderer.cpp

SOURCES = $(SRC_DIR)/main.cpp $(ENGINE_SOURCES)
SOURCES_EDIT = $(SRC_DIR)/edit.cpp $(ENGINE_SOURCES)

all: deps $(TARGET) $(TARGET_EDIT)

deps:
	@if [ ! -d "$(GLFW_BUILD_DIR)" ] || [ ! -d "$(FREETYPE_BUILD_DIR)" ] || [ ! -d "$(HARFBUZZ_BUILD_DIR)" ]; then \
		echo "Setting up text rendering dependencies..."; \
		./setup_deps.sh; \
	fi

$(TARGET): $(SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

$(TARGET_EDIT): $(SOURCES_EDIT) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES_EDIT) -o $(TARGET_EDIT) $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(TARGET) $(TARGET_EDIT)
	rmdir $(BUILD_DIR) 2>/dev/null || true

clean-all: clean
	rm -rf $(VENDOR_DIR)

release:
	$(MAKE) OPT=1

.PHONY: all deps clean clean-all release