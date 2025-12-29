#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GLFW_SOURCE_DIR="$SCRIPT_DIR/glfw-3.4"
GLFW_BUILD_DIR="$SCRIPT_DIR/glfw-build"
CMAKE_DIR="$SCRIPT_DIR/cmake"
CMAKE_BIN="$CMAKE_DIR/CMake.app/Contents/bin/cmake"

echo "Setting up GLFW dependencies..."
echo "GLFW source: $GLFW_SOURCE_DIR"
echo "GLFW build: $GLFW_BUILD_DIR"

# Check for cmake and download if needed
if ! command -v cmake &> /dev/null && [ ! -f "$CMAKE_BIN" ]; then
    echo "Downloading cmake binary for macOS..."
    mkdir -p "$CMAKE_DIR"
    cd "$CMAKE_DIR"
    
    CMAKE_VERSION="3.28.3"
    CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-macos-universal.tar.gz"
    
    curl -L "$CMAKE_URL" -o cmake.tar.gz
    tar -xzf cmake.tar.gz --strip-components=1
    rm cmake.tar.gz
    
    echo "CMake downloaded and extracted to $CMAKE_DIR"
fi

# Set cmake command
if [ -f "$CMAKE_BIN" ]; then
    CMAKE_CMD="$CMAKE_BIN"
else
    CMAKE_CMD="cmake"
fi

# Check if GLFW source exists
if [ ! -d "$GLFW_SOURCE_DIR" ]; then
    echo "Error: GLFW source directory not found at $GLFW_SOURCE_DIR"
    exit 1
fi

# Create build directory
mkdir -p "$GLFW_BUILD_DIR"

# Configure and build GLFW
cd "$GLFW_BUILD_DIR"

echo "Configuring GLFW with CMake..."
"$CMAKE_CMD" "$GLFW_SOURCE_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGLFW_BUILD_EXAMPLES=OFF \
    -DGLFW_BUILD_TESTS=OFF \
    -DGLFW_BUILD_DOCS=OFF \
    -DCMAKE_INSTALL_PREFIX="$GLFW_BUILD_DIR/install"

echo "Building GLFW..."
make -j$(sysctl -n hw.ncpu)

echo "Installing GLFW to local directory..."
make install

echo ""
echo "GLFW setup complete!"
echo "Headers: $GLFW_BUILD_DIR/install/include"
echo "Libraries: $GLFW_BUILD_DIR/install/lib"
echo ""
echo "You can now build the mdeditor application with: make"