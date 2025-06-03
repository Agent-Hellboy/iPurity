#!/bin/bash

set -e

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if a Homebrew package is installed
brew_package_exists() {
    brew list "$1" >/dev/null 2>&1
}

# Function to install a Homebrew package if not already installed
install_brew_package() {
    local package=$1
    if ! brew_package_exists "$package"; then
        echo "Installing $package via Homebrew..."
        brew install "$package"
    else
        echo "$package is already installed."
    fi
}

# Function to install FlatBuffers v24.3.25 from source
install_flatbuffers() {
    local install_dir="$1"
    echo "=== Installing FlatBuffers v24.3.25 from source ==="
    
    # Create a temporary directory for building
    local tmpdir=$(mktemp -d /tmp/flatbuffers_install.XXXXXX)
    cd "$tmpdir"
    
    echo "Cloning FlatBuffers v24.3.25..."
    git clone https://github.com/google/flatbuffers.git flatbuffers_src
    cd flatbuffers_src
    git checkout v24.3.25
    
    echo "Building and installing FlatBuffers..."
    mkdir -p build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="${install_dir}" \
          ..
    cmake --build . --target install
    
    # Clean up
    cd /tmp
    rm -rf "$tmpdir"
    
    echo "FlatBuffers v24.3.25 installed to ${install_dir}"
}

# Determine install prefix (use /opt/homebrew on Apple Silicon, otherwise /usr/local)
if [[ "$(uname -m)" == "arm64" ]]; then
    INSTALL_PREFIX="/opt/homebrew"
else
    INSTALL_PREFIX="/usr/local"
fi

echo "=== Checking and installing system dependencies ==="

# Check for Homebrew
if ! command_exists brew; then
    echo "Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo "Homebrew is already installed."
fi

# Update Homebrew
echo "Updating Homebrew..."
brew update

# Install required packages
echo "Installing required packages via Homebrew..."
install_brew_package pkg-config
install_brew_package cmake
install_brew_package make
install_brew_package bazel
install_brew_package opencv
install_brew_package libimobiledevice
install_brew_package libplist

# Install FlatBuffers v24.3.25 from source
install_flatbuffers "${INSTALL_PREFIX}"

# Verify pkg-config and OpenCV
if ! pkg-config --exists opencv4 && ! pkg-config --exists opencv; then
    echo "Error: OpenCV not found after installation. Please try:"
    echo "brew reinstall opencv"
    exit 1
fi

echo "=== Installing TensorFlow Lite under ${INSTALL_PREFIX} ==="

# Create a temporary build directory
TMPDIR=$(mktemp -d /tmp/tflite_install.XXXXXX)
cd "$TMPDIR"

# Clone TensorFlow (we need its C++ headers and build system for TFLite)
echo "Cloning TensorFlow (for TFLite) ..."
git clone --depth 1 https://github.com/tensorflow/tensorflow.git
cd tensorflow

# Configure TensorFlow
echo "Configuring TensorFlow..."
export TF_NEED_CUDA=0
export TF_NEED_ROCM=0
export TF_NEED_OPENCL_SYCL=0
export TF_ENABLE_XLA=0
export TF_NEED_TENSORRT=0
yes "" | ./configure

# Build TFLite using Bazel
echo "Building TensorFlow Lite via Bazel..."
if [[ "$(uname -m)" == "arm64" ]]; then
    bazel build -c opt //tensorflow/lite:libtensorflowlite.dylib
    TFLITE_LIB="libtensorflowlite.dylib"
else
    bazel build -c opt //tensorflow/lite:libtensorflowlite.so
    TFLITE_LIB="libtensorflowlite.so"
fi

# Create directories under INSTALL_PREFIX if they don't exist
mkdir -p "${INSTALL_PREFIX}/include/tensorflow/lite"
mkdir -p "${INSTALL_PREFIX}/lib"

# Copy TFLite library
echo "Copying ${TFLITE_LIB} to ${INSTALL_PREFIX}/lib ..."
cp "bazel-bin/tensorflow/lite/${TFLITE_LIB}" "${INSTALL_PREFIX}/lib/"

# Copy TFLite headers
echo "Copying TFLite headers to ${INSTALL_PREFIX}/include/tensorflow/lite ..."
cp -r tensorflow/lite/*.h "${INSTALL_PREFIX}/include/tensorflow/lite/"

# Clean up temporary build directory
echo "Cleaning up temporary build directory ..."
rm -rf "$TMPDIR"

# Create symlinks for versioned libraries if needed
if [ -d "${INSTALL_PREFIX}/lib" ]; then
    echo "Checking for versioned libraries in ${INSTALL_PREFIX}/lib..."
    pushd "${INSTALL_PREFIX}/lib" >/dev/null || exit 1

    # Symlink for libimobiledevice
    IMOBILE=$(ls libimobiledevice-*.dylib 2>/dev/null | head -n 1)
    if [[ -n "$IMOBILE" ]]; then
        echo "Creating symlink libimobiledevice.dylib → $IMOBILE"
        ln -sf "$IMOBILE" libimobiledevice.dylib
    fi

    # Symlink for libplist
    PLIST=$(ls libplist-*.dylib 2>/dev/null | grep -v '++' | head -n 1)
    if [[ -n "$PLIST" ]]; then
        echo "Creating symlink libplist.dylib → $PLIST"
        ln -sf "$PLIST" libplist.dylib
    fi

    # Symlink for libplist++
    PLISTPP=$(ls libplist++-*.dylib 2>/dev/null | head -n 1)
    if [[ -n "$PLISTPP" ]]; then
        echo "Creating symlink libplist++.dylib → $PLISTPP"
        ln -sf "$PLISTPP" libplist++.dylib
    fi

    popd >/dev/null
fi

echo "=== Installation Summary ==="
echo "✓ Homebrew packages installed/verified:"
echo "  - pkg-config"
echo "  - cmake"
echo "  - make"
echo "  - bazel"
echo "  - opencv"
echo "  - libimobiledevice"
echo "  - libplist"
echo "✓ FlatBuffers v24.3.25 installed from source under ${INSTALL_PREFIX}"
echo "✓ TensorFlow Lite installed under ${INSTALL_PREFIX}"
echo "✓ Library symlinks created in ${INSTALL_PREFIX}/lib"
echo ""
echo "Installation complete! You can now build iPurity using CMake."
echo "Run './configure' to verify the installation." 