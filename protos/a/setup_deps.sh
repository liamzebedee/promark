#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GLFW_SOURCE_DIR="$SCRIPT_DIR/glfw-3.4"
GLFW_BUILD_DIR="$SCRIPT_DIR/glfw-build"
FREETYPE_SOURCE_DIR="$SCRIPT_DIR/freetype"
FREETYPE_BUILD_DIR="$SCRIPT_DIR/freetype-build"
HARFBUZZ_SOURCE_DIR="$SCRIPT_DIR/harfbuzz"
HARFBUZZ_BUILD_DIR="$SCRIPT_DIR/harfbuzz-build"
CMAKE_DIR="$SCRIPT_DIR/cmake"
CMAKE_BIN="$CMAKE_DIR/CMake.app/Contents/bin/cmake"

echo "Setting up text rendering dependencies..."
echo "GLFW source: $GLFW_SOURCE_DIR"
echo "GLFW build: $GLFW_BUILD_DIR"
echo "FreeType source: $FREETYPE_SOURCE_DIR"
echo "FreeType build: $FREETYPE_BUILD_DIR"
echo "HarfBuzz source: $HARFBUZZ_SOURCE_DIR"
echo "HarfBuzz build: $HARFBUZZ_BUILD_DIR"

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

# Check if source directories exist
if [ ! -d "$GLFW_SOURCE_DIR" ]; then
    echo "Error: GLFW source directory not found at $GLFW_SOURCE_DIR"
    exit 1
fi

if [ ! -d "$FREETYPE_SOURCE_DIR" ]; then
    echo "Error: FreeType source directory not found at $FREETYPE_SOURCE_DIR"
    exit 1
fi

if [ ! -d "$HARFBUZZ_SOURCE_DIR" ]; then
    echo "Error: HarfBuzz source directory not found at $HARFBUZZ_SOURCE_DIR"
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

# Build FreeType
echo ""
echo "Building FreeType..."
mkdir -p "$FREETYPE_BUILD_DIR"
cd "$FREETYPE_BUILD_DIR"

"$CMAKE_CMD" "$FREETYPE_SOURCE_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DFT_DISABLE_ZLIB=TRUE \
    -DFT_DISABLE_BZIP2=TRUE \
    -DFT_DISABLE_PNG=TRUE \
    -DFT_DISABLE_HARFBUZZ=TRUE \
    -DFT_DISABLE_BROTLI=TRUE \
    -DCMAKE_INSTALL_PREFIX="$FREETYPE_BUILD_DIR/install"

make -j$(sysctl -n hw.ncpu)
make install

# Build HarfBuzz
echo ""
echo "Building HarfBuzz..."
mkdir -p "$HARFBUZZ_BUILD_DIR"
cd "$HARFBUZZ_BUILD_DIR"

"$CMAKE_CMD" "$HARFBUZZ_SOURCE_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DHB_HAVE_FREETYPE=ON \
    -DFREETYPE_INCLUDE_DIRS="$FREETYPE_BUILD_DIR/install/include/freetype2" \
    -DFREETYPE_LIBRARY="$FREETYPE_BUILD_DIR/install/lib/libfreetype.a" \
    -DCMAKE_INSTALL_PREFIX="$HARFBUZZ_BUILD_DIR/install"

make -j$(sysctl -n hw.ncpu)
make install

echo ""
echo "All dependencies setup complete!"
echo "GLFW - Headers: $GLFW_BUILD_DIR/install/include | Libraries: $GLFW_BUILD_DIR/install/lib"
echo "FreeType - Headers: $FREETYPE_BUILD_DIR/install/include | Libraries: $FREETYPE_BUILD_DIR/install/lib"
echo "HarfBuzz - Headers: $HARFBUZZ_BUILD_DIR/install/include | Libraries: $HARFBUZZ_BUILD_DIR/install/lib"
echo ""
echo "You can now build the mdeditor application with: make"