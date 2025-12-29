CXX = g++
VENDOR_DIR = vendor
GLFW_BUILD_DIR = $(VENDOR_DIR)/glfw-build
FREETYPE_BUILD_DIR = $(VENDOR_DIR)/freetype-build
HARFBUZZ_BUILD_DIR = $(VENDOR_DIR)/harfbuzz-build
SRC_DIR = src
BUILD_DIR = build

CXXFLAGS = -std=c++17 -Wall -Wextra \
	-I$(SRC_DIR) \
	-I$(GLFW_BUILD_DIR)/install/include \
	-I$(FREETYPE_BUILD_DIR)/install/include/freetype2 \
	-I$(HARFBUZZ_BUILD_DIR)/install/include/harfbuzz

LDFLAGS = -L$(GLFW_BUILD_DIR)/install/lib \
	-L$(FREETYPE_BUILD_DIR)/install/lib \
	-L$(HARFBUZZ_BUILD_DIR)/install/lib \
	-lglfw3 -lfreetype -lharfbuzz \
	-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

TARGET = $(BUILD_DIR)/mdeditor
SOURCES = $(SRC_DIR)/main.cpp $(SRC_DIR)/engine/engine.cpp

all: deps $(TARGET)

deps:
	@if [ ! -d "$(GLFW_BUILD_DIR)" ] || [ ! -d "$(FREETYPE_BUILD_DIR)" ] || [ ! -d "$(HARFBUZZ_BUILD_DIR)" ]; then \
		echo "Setting up text rendering dependencies..."; \
		./setup_deps.sh; \
	fi

$(TARGET): $(SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(TARGET)
	rmdir $(BUILD_DIR) 2>/dev/null || true

clean-all: clean
	rm -rf $(VENDOR_DIR)

.PHONY: all deps clean clean-all