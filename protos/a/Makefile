CXX = g++
GLFW_BUILD_DIR = glfw-build
CXXFLAGS = -std=c++17 -Wall -Wextra -I$(GLFW_BUILD_DIR)/install/include
LDFLAGS = -L$(GLFW_BUILD_DIR)/install/lib -lglfw3 -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

TARGET = mdeditor
SOURCES = main.cpp engine.cpp

all: deps $(TARGET)

deps:
	@if [ ! -d "$(GLFW_BUILD_DIR)" ]; then \
		echo "Setting up GLFW dependencies..."; \
		./setup_deps.sh; \
	fi

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

clean-all: clean
	rm -rf $(GLFW_BUILD_DIR)

.PHONY: all deps clean clean-all