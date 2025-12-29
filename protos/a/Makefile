CXX = g++
GLFW_BUILD_DIR = glfw-build
FREETYPE_BUILD_DIR = freetype-build
HARFBUZZ_BUILD_DIR = harfbuzz-build

CXXFLAGS = -std=c++17 -Wall -Wextra \
	-I$(GLFW_BUILD_DIR)/install/include \
	-I$(FREETYPE_BUILD_DIR)/install/include/freetype2 \
	-I$(HARFBUZZ_BUILD_DIR)/install/include/harfbuzz

LDFLAGS = -L$(GLFW_BUILD_DIR)/install/lib \
	-L$(FREETYPE_BUILD_DIR)/install/lib \
	-L$(HARFBUZZ_BUILD_DIR)/install/lib \
	-lglfw3 -lfreetype -lharfbuzz \
	-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

TARGET = mdeditor
SOURCES = main.cpp engine.cpp

all: deps $(TARGET)

deps:
	@if [ ! -d "$(GLFW_BUILD_DIR)" ] || [ ! -d "$(FREETYPE_BUILD_DIR)" ] || [ ! -d "$(HARFBUZZ_BUILD_DIR)" ]; then \
		echo "Setting up text rendering dependencies..."; \
		./setup_deps.sh; \
	fi

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

clean-all: clean
	rm -rf $(GLFW_BUILD_DIR) $(FREETYPE_BUILD_DIR) $(HARFBUZZ_BUILD_DIR)

.PHONY: all deps clean clean-all