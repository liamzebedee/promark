# Enable parallel compilation by default using all CPU cores
MAKEFLAGS += -j$(shell nproc)

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

# Source files
SOURCES = $(SRC_DIR)/main.cpp $(ENGINE_SRC)
SOURCES_EDIT = $(SRC_DIR)/edit.cpp $(ENGINE_SRC)

# Convert sources to object files (in build directory)
OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
OBJECTS_EDIT = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES_EDIT))

# Test sources
TEST_DIR = tests
TEST_SOURCES = $(TEST_DIR)/run_tests.cpp \
               $(TEST_DIR)/test_helpers.cpp \
               $(TEST_DIR)/test_basic_formatting.cpp \
               $(TEST_DIR)/test_inline_block.cpp \
               $(TEST_DIR)/test_block_layout.cpp \
               $(TEST_DIR)/test_cursor_positioning.cpp \
               $(TEST_DIR)/test_mouse_click.cpp \
               $(TEST_DIR)/test_bug_caret_after_typing.cpp \
               $(TEST_DIR)/test_visual_line_navigation.cpp \
               $(TEST_DIR)/test_sticky_caret.cpp \
               $(TEST_DIR)/test_sticky_diagnostic.cpp \
               $(TEST_DIR)/test_sticky_debug.cpp \
               $(TEST_DIR)/test_navigation_trace.cpp \
               $(TEST_DIR)/test_navigation_middle.cpp \
               $(TEST_DIR)/test_nav_click_x.cpp \
               $(TEST_DIR)/test_up_nav.cpp \
               $(TEST_DIR)/test_scroll_visual_mode.cpp \
               $(TEST_DIR)/test_sticky_verification.cpp \
               $(TEST_DIR)/test_keyboard_shortcuts.cpp \
               $(TEST_DIR)/test_home_end_keys.cpp \
               $(TEST_DIR)/test_visual_caret_bug.cpp \
               $(TEST_DIR)/test_scroll_caret_bug.cpp \
               $(TEST_DIR)/test_scrollbar.cpp \
               $(TEST_DIR)/test_click_edit_blocks.cpp \
               $(TEST_DIR)/test_long_document_navigation.cpp \
               $(TEST_DIR)/test_blockquote_caret.cpp \
               $(TEST_DIR)/test_heading_paragraph_nav.cpp \
               $(TEST_DIR)/test_mode_switch.cpp \
               $(TEST_DIR)/test_raw_mode_empty_lines.cpp \
               $(TEST_DIR)/test_typography.cpp \
               $(TEST_DIR)/test_formatting_visual.cpp \
               $(TEST_DIR)/test_formatting_render.cpp \
               $(TEST_DIR)/test_e2e_comprehensive.cpp \
               $(TEST_DIR)/test_formatting_spacing.cpp \
               $(TEST_DIR)/test_parser_debug.cpp \
               $(TEST_DIR)/test_visual_mode_empty_lines.cpp \
               $(TEST_DIR)/test_link_in_bold_debug.cpp

# Test objects (engine objects + test-specific objects)
ENGINE_OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRC))
TEST_OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(TEST_SOURCES))

# Test-specific flags (need stb_image_write.h from GLFW deps)
TEST_CXXFLAGS = $(CXXFLAGS) -I$(VENDOR_DIR)/glfw-3.4/deps

all: deps $(TARGET) $(TARGET_EDIT)

deps:
	@if [ ! -d "$(GLFW_BUILD_DIR)" ] || [ ! -d "$(FREETYPE_BUILD_DIR)" ] || [ ! -d "$(HARFBUZZ_BUILD_DIR)" ]; then \
		echo "Setting up text rendering dependencies..."; \
		./setup_deps.sh; \
	fi

# Link targets from object files
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_EDIT): $(OBJECTS_EDIT)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_TESTS): $(TEST_OBJECTS) $(ENGINE_OBJECTS)
	$(CXX) $(TEST_CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Pattern rule for compiling source files to object files
$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Pattern rule for test files (with TEST_CXXFLAGS)
$(BUILD_DIR)/$(TEST_DIR)/%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(TEST_CXXFLAGS) -c $< -o $@

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
