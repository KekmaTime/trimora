#!/bin/bash

set -e

echo "========================================="
echo "Trimora - Build Setup Script"
echo "========================================="
echo ""

# Check for required tools
echo "Checking for required tools..."

if ! command -v cmake &> /dev/null; then
    echo "Error: cmake not found. Please install CMake 3.20+"
    exit 1
fi

if ! command -v ninja &> /dev/null; then
    echo "Warning: ninja not found. Will use default generator."
    GENERATOR=""
else
    GENERATOR="-G Ninja"
fi

if ! command -v ffmpeg &> /dev/null; then
    echo "Warning: ffmpeg not found in PATH. The application will not work without it."
    echo "Install ffmpeg: sudo pacman -S ffmpeg  (Arch) or  sudo apt install ffmpeg  (Ubuntu)"
fi

# Check for ImGui
echo ""
echo "Checking for ImGui..."
if [ ! -d "external/imgui" ]; then
    echo "ImGui not found. Cloning from GitHub..."
    mkdir -p external
    git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git external/imgui
    echo "ImGui cloned successfully."
else
    echo "ImGui found."
fi

# Check for nativefiledialog-extended
echo ""
echo "Checking for nativefiledialog-extended..."
if [ ! -d "external/nfd" ]; then
    echo "NFD not found. Cloning from GitHub..."
    mkdir -p external
    git clone --depth 1 https://github.com/btzy/nativefiledialog-extended.git external/nfd
    echo "NFD cloned successfully."
else
    echo "NFD found."
fi

# Create build directory
echo ""
echo "Creating build directory..."
mkdir -p build
cd build

# Run CMake
echo ""
echo "Running CMake..."
cmake .. $GENERATOR \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo ""
echo "========================================="
echo "Setup complete!"
echo "========================================="
echo ""
echo "To build the project:"
echo "  cd build"
if [ -n "$GENERATOR" ]; then
    echo "  ninja"
else
    echo "  make -j$(nproc)"
fi
echo ""
echo "To run:"
echo "  ./trimora"
echo ""

