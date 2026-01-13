#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENDOR_DIR="$SCRIPT_DIR/vendor"
GLFW_SOURCE_DIR="$VENDOR_DIR/glfw-3.4"
GLFW_BUILD_DIR="$VENDOR_DIR/glfw-build"
FREETYPE_SOURCE_DIR="$VENDOR_DIR/freetype"
FREETYPE_BUILD_DIR="$VENDOR_DIR/freetype-build"
HARFBUZZ_SOURCE_DIR="$VENDOR_DIR/harfbuzz"
HARFBUZZ_BUILD_DIR="$VENDOR_DIR/harfbuzz-build"
LIBJPEG_SOURCE_DIR="$VENDOR_DIR/libjpeg-turbo"
LIBJPEG_BUILD_DIR="$VENDOR_DIR/libjpeg-turbo-build"
CMAKE_DIR="$VENDOR_DIR/cmake"

# Platform detection
OS="$(uname -s)"
case "$OS" in
    Darwin)
        CMAKE_BIN="$CMAKE_DIR/CMake.app/Contents/bin/cmake"
        NPROC="sysctl -n hw.ncpu"
        ;;
    Linux)
        CMAKE_BIN="$CMAKE_DIR/bin/cmake"
        NPROC="nproc"
        ;;
    *)
        echo "Unsupported OS: $OS"
        exit 1
        ;;
esac

echo "Setting up text rendering dependencies on $OS..."
echo "GLFW source: $GLFW_SOURCE_DIR"
echo "GLFW build: $GLFW_BUILD_DIR"
echo "FreeType source: $FREETYPE_SOURCE_DIR"
echo "FreeType build: $FREETYPE_BUILD_DIR"
echo "HarfBuzz source: $HARFBUZZ_SOURCE_DIR"
echo "HarfBuzz build: $HARFBUZZ_BUILD_DIR"
echo "libjpeg-turbo source: $LIBJPEG_SOURCE_DIR"
echo "libjpeg-turbo build: $LIBJPEG_BUILD_DIR"

# Check for cmake and download if needed
if ! command -v cmake &> /dev/null && [ ! -f "$CMAKE_BIN" ]; then
    echo "Downloading cmake binary for $OS..."
    mkdir -p "$CMAKE_DIR"
    cd "$CMAKE_DIR"

    CMAKE_VERSION="3.28.3"
    case "$OS" in
        Darwin)
            CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-macos-universal.tar.gz"
            ;;
        Linux)
            CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
            ;;
    esac

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

if [ ! -d "$LIBJPEG_SOURCE_DIR" ]; then
    echo "Error: libjpeg-turbo source directory not found at $LIBJPEG_SOURCE_DIR"
    exit 1
fi

# Get number of CPU cores for parallel build
NUM_JOBS=$($NPROC)

# Build GLFW
echo ""
echo "Building GLFW..."
mkdir -p "$GLFW_BUILD_DIR"
cd "$GLFW_BUILD_DIR"

"$CMAKE_CMD" "$GLFW_SOURCE_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGLFW_BUILD_EXAMPLES=OFF \
    -DGLFW_BUILD_TESTS=OFF \
    -DGLFW_BUILD_DOCS=OFF \
    -DCMAKE_INSTALL_PREFIX="$GLFW_BUILD_DIR/install"

make -j$NUM_JOBS
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

make -j$NUM_JOBS
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

make -j$NUM_JOBS
make install

# Build libjpeg-turbo
echo ""
echo "Building libjpeg-turbo..."
mkdir -p "$LIBJPEG_BUILD_DIR"
cd "$LIBJPEG_BUILD_DIR"

"$CMAKE_CMD" "$LIBJPEG_SOURCE_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_SHARED=OFF \
    -DENABLE_STATIC=ON \
    -DCMAKE_INSTALL_PREFIX="$LIBJPEG_BUILD_DIR/install"

make -j$NUM_JOBS
make install

echo ""
echo "All dependencies setup complete!"
echo "GLFW - Headers: $GLFW_BUILD_DIR/install/include | Libraries: $GLFW_BUILD_DIR/install/lib"
echo "FreeType - Headers: $FREETYPE_BUILD_DIR/install/include | Libraries: $FREETYPE_BUILD_DIR/install/lib"
echo "HarfBuzz - Headers: $HARFBUZZ_BUILD_DIR/install/include | Libraries: $HARFBUZZ_BUILD_DIR/install/lib"
echo "libjpeg-turbo - Headers: $LIBJPEG_BUILD_DIR/install/include | Libraries: $LIBJPEG_BUILD_DIR/install/lib"
echo ""
echo "You can now build the mdeditor application with: make"
