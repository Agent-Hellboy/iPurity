#!/bin/bash
set -e

# ------------------------------------------------------------
# Helper Functions
# ------------------------------------------------------------
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

brew_package_exists() {
    brew list "$1" >/dev/null 2>&1
}

install_brew_package() {
    local pkg="$1"
    if ! brew_package_exists "$pkg"; then
        echo "→ Installing Homebrew package: $pkg"
        brew install "$pkg"
    else
        echo "→ Homebrew package already present: $pkg"
    fi
}

install_flatbuffers() {
    local install_dir="$1"
    echo "=== Installing FlatBuffers v24.3.25 from source into ${install_dir} ==="
    local tmpdir
    tmpdir=$(mktemp -d /tmp/flatbuffers_install.XXXXXX)
    echo "→ Cloning FlatBuffers into temporary dir: $tmpdir"
    git clone https://github.com/google/flatbuffers.git "$tmpdir/flatbuffers_src"
    pushd "$tmpdir/flatbuffers_src" >/dev/null
    git checkout v24.3.25

    echo "→ Running CMake (FlatBuffers)..."
    mkdir -p build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="${install_dir}" \
          ..
    echo "→ Building & installing FlatBuffers (requires sudo to write under ${install_dir})..."
    sudo cmake --build . --target install

    popd >/dev/null
    echo "→ Cleaning up FlatBuffers temp dir (using sudo because some build artifacts are root-owned)"
    sudo rm -rf "$tmpdir"
    echo "→ FlatBuffers v24.3.25 installed to ${install_dir}"
}

# ------------------------------------------------------------
# Main Installer
# ------------------------------------------------------------
printf "\n=== iPurity/macOS Dependency Installer ===\n\n"

# 1) Determine Homebrew prefix (Intel vs Apple Silicon)
if [[ "$(uname -m)" == "arm64" ]]; then
    INSTALL_PREFIX="/opt/homebrew"
else
    INSTALL_PREFIX="/usr/local"
fi
echo "→ Homebrew prefix detected: ${INSTALL_PREFIX}"

# 2) Ensure Homebrew is installed
if ! command_exists brew; then
    echo "→ Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo "→ Homebrew already present."
fi

echo "→ Updating Homebrew ..."
brew update

# 3) Install required packages via Homebrew
echo "→ Installing pkg-config, cmake, make, bazelisk, opencv, libimobiledevice, libplist"
install_brew_package pkg-config
install_brew_package cmake
install_brew_package make
install_brew_package bazelisk
install_brew_package opencv
install_brew_package libimobiledevice
install_brew_package libplist

# 4) If Homebrew’s "bazel" formula exists, unlink it so it does not shadow bazelisk
if brew_package_exists bazel; then
    echo "→ Homebrew 'bazel' is installed; unlinking to avoid conflicts with bazelisk"
    brew unlink bazel || true
fi

# 5) Create a symlink so that 'bazel' points to 'bazelisk'
BREWISK_BIN="$(brew --prefix bazelisk)/bin/bazelisk"
TARGET_BAZEL_SYMLINK="${INSTALL_PREFIX}/bin/bazel"

echo "→ Ensuring that 'bazel' → 'bazelisk' symlink exists:"
echo "    bazelisk path:   ${BREWISK_BIN}"
echo "    desired symlink: ${TARGET_BAZEL_SYMLINK}"
if [[ ! -f "$BREWISK_BIN" ]]; then
    echo "Error: bazelisk binary not found at: ${BREWISK_BIN}"
    echo "Make sure 'brew install bazelisk' succeeded."
    exit 1
fi

# Make sure the directory exists
mkdir -p "$(dirname "$TARGET_BAZEL_SYMLINK")"

# Create or overwrite the symlink:
ln -sf "$BREWISK_BIN" "$TARGET_BAZEL_SYMLINK"
echo "→ Created symlink: bazel → bazelisk"

# 6) Verify pkg-config and OpenCV
echo "→ Verifying pkg-config + OpenCV ..."
if ! pkg-config --exists opencv4 && ! pkg-config --exists opencv; then
    echo "Error: OpenCV not detected by pkg-config."
    echo "Run: brew reinstall opencv"
    exit 1
fi
echo "→ pkg-config and OpenCV checks passed."

# 7) Install FlatBuffers v24.3.25 from source
install_flatbuffers "${INSTALL_PREFIX}"

# 8) Build and install TensorFlow Lite (non-interactive)
echo "=== Installing TensorFlow Lite under ${INSTALL_PREFIX} ==="
TMPDIR=$(mktemp -d /tmp/tflite_install.XXXXXX)
echo "→ Created temp dir: $TMPDIR"
git clone --depth 1 https://github.com/tensorflow/tensorflow.git "$TMPDIR/tensorflow"
pushd "$TMPDIR/tensorflow" >/dev/null

echo "→ Running TensorFlow's ./configure (non-interactive) ..."
export TF_NEED_CUDA=0
export TF_NEED_ROCM=0
export TF_NEED_OPENCL_SYCL=0
export TF_ENABLE_XLA=0
export TF_NEED_TENSORRT=0

yes "" | ./configure 2>&1 | tee ../configure-tf.log

echo "→ Building TensorFlow Lite via bazelisk ..."
bazel build -c opt //tensorflow/lite:libtensorflowlite.dylib 2>&1 | tee ../tflite-build.log

echo "→ Copying TensorFlow Lite artifacts into ${INSTALL_PREFIX} ..."
mkdir -p "${INSTALL_PREFIX}/include/tensorflow/lite"
mkdir -p "${INSTALL_PREFIX}/lib"

sudo cp "bazel-bin/tensorflow/lite/libtensorflowlite.dylib" "${INSTALL_PREFIX}/lib/"
sudo cp -r tensorflow/lite/*.h "${INSTALL_PREFIX}/include/tensorflow/lite/"

# —––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––—
# Copy over the missing MLIR “lite” headers (allocation.h, etc.) from
# the TensorFlow source into INSTALL_PREFIX.  
# These live under tensorflow/compiler/mlir/lite/ in the TF repo.
echo "→ Copying MLIR-lit e headers (tensorflow/compiler/mlir/lite/…) into ${INSTALL_PREFIX}/include"
sudo mkdir -p "${INSTALL_PREFIX}/include/tensorflow/compiler/mlir/lite"
sudo cp -r tensorflow/compiler/mlir/lite/* "${INSTALL_PREFIX}/include/tensorflow/compiler/mlir/lite/"

echo "→ Cleaning up TensorFlow temp dir"
popd >/dev/null
sudo rm -rf "$TMPDIR"

# 9) Create unversioned symlinks (libimobiledevice & libplist)
echo "→ Checking for versioned libraries in ${INSTALL_PREFIX}/lib ..."
pushd "${INSTALL_PREFIX}/lib" >/dev/null || exit 1

IMOBILE_VER=$(ls libimobiledevice-*.dylib 2>/dev/null | head -n1)
if [[ -n "$IMOBILE_VER" ]]; then
    echo "   Found $IMOBILE_VER → symlinking to libimobiledevice.dylib"
    sudo ln -sf "$IMOBILE_VER" libimobiledevice.dylib
fi

PLIST_VER=$(ls libplist-*.dylib 2>/dev/null | grep -v '++' | head -n1)
if [[ -n "$PLIST_VER" ]]; then
    echo "   Found $PLIST_VER → symlinking to libplist.dylib"
    sudo ln -sf "$PLIST_VER" libplist.dylib
fi

PLISTPP_VER=$(ls libplist++-*.dylib 2>/dev/null | head -n1)
if [[ -n "$PLISTPP_VER" ]]; then
    echo "   Found $PLISTPP_VER → symlinking to libplist++.dylib"
    sudo ln -sf "$PLISTPP_VER" libplist++.dylib
fi

popd >/dev/null

# ------------------------------------------------------------
# Final summary
# ------------------------------------------------------------
echo ""
echo "=== Installation Summary ==="
echo " • pkg-config installed"
echo " • cmake installed"
echo " • make installed"
echo " • bazelisk installed"
echo " • bazel → bazelisk symlink created"
echo " • opencv installed"
echo " • libimobiledevice installed"
echo " • libplist installed"
echo " • FlatBuffers v24.3.25 installed at ${INSTALL_PREFIX}"
echo " • TensorFlow Lite installed under ${INSTALL_PREFIX}"
echo " • MLIR headers (tensorflow/compiler/mlir/lite/…) copied into ${INSTALL_PREFIX}/include"
echo " • Symlinks created for libimobiledevice & libplist in ${INSTALL_PREFIX}/lib"
echo ""
echo "Done! Now you can run:"
echo "    cd <iPurity source dir>"
echo "    mkdir -p build && cd build"
echo "    cmake .."
echo "    make"
echo ""
exit 0
