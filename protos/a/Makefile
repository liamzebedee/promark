CXX = g++
VENDOR_DIR = vendor
GLFW_BUILD_DIR = $(VENDOR_DIR)/glfw-build
FREETYPE_BUILD_DIR = $(VENDOR_DIR)/freetype-build
HARFBUZZ_BUILD_DIR = $(VENDOR_DIR)/harfbuzz-build
LIBJPEG_BUILD_DIR = $(VENDOR_DIR)/libjpeg-turbo-build
SRC_DIR = src
BUILD_DIR = build

# Shared source list
include sources.mk

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS_PLATFORM = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else
    LDFLAGS_PLATFORM = -lGL -lX11 -lpthread -ldl
endif

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
	$(LDFLAGS_PLATFORM)

TARGET = $(BUILD_DIR)/mdeditor
TARGET_EDIT = $(BUILD_DIR)/mdedit
TARGET_TESTS = $(BUILD_DIR)/run_tests

SOURCES = $(SRC_DIR)/main.cpp $(ENGINE_SRC)
SOURCES_EDIT = $(SRC_DIR)/edit.cpp $(ENGINE_SRC)

# Test sources
TEST_DIR = tests
TEST_SOURCES = $(TEST_DIR)/run_tests.cpp \
               $(TEST_DIR)/test_helpers.cpp \
               $(TEST_DIR)/test_basic_formatting.cpp \
               $(TEST_DIR)/test_inline_block.cpp \
               $(TEST_DIR)/test_block_layout.cpp \
               $(TEST_DIR)/test_cursor_positioning.cpp \
               $(TEST_DIR)/test_mouse_click.cpp

# Test-specific flags (need stb_image_write.h from GLFW deps)
TEST_CXXFLAGS = $(CXXFLAGS) -I$(VENDOR_DIR)/glfw-3.4/deps

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

$(TARGET_TESTS): $(TEST_SOURCES) $(ENGINE_SRC) | $(BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_SOURCES) $(ENGINE_SRC) -o $(TARGET_TESTS) $(LDFLAGS)

test: deps $(TARGET_TESTS)
	@echo "Running visual tests..."
	@cd $(BUILD_DIR) && ./run_tests

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	cp -r fonts $(BUILD_DIR)/

clean:
	rm -rf $(BUILD_DIR)

clean-all: clean
	rm -rf $(VENDOR_DIR)

release:
	$(MAKE) OPT=1

.PHONY: all deps clean clean-all release test