# iPurity

[![Build](https://github.com/Agent-Hellboy/iPurity/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/Agent-Hellboy/iPurity/actions/workflows/c-cpp.yml)

iPurity is a simple NSFW (Not Safe For Work) detector for iOS devices.

It utilizes AFC (Apple File Conduit) to list and open each media file and OpenCV to detect NSFW images.

> **Note**: Tested only on Apple Silicon Mac

## Why did I make this?
- The program was created because one of my younger siblings accidentally encountered NSFW content.
- There is often no time to manually check all images on their phone before passing it to a younger sibling.
- This program aims to assist in identifying potentially inappropriate content.

## Disclaimer
- There may be many false negatives; however, it can help reduce the dataset to scan by approximately 90–95%.

## Prerequisites

Before building iPurity from source, ensure you have the following installed:

- **Homebrew** (for macOS package management)
- **libimobiledevice** (C library and headers)
- **libplist** (C library and headers)
- **OpenCV** (C++ library and headers)
- **TensorFlow Lite** (C++ library and headers, built via Bazel)
- **Bazel** (to build TensorFlow Lite)
- **CMake** (≥ 3.10)
- **Make**
- **FlatBuffers** (via Homebrew)

Below are installation steps to get each prerequisite in place.

### 1. Install Homebrew (if not already installed)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 2. Install system‐wide dependencies via Homebrew

```bash
brew install libimobiledevice libplist opencv flatbuffers cmake make bazel
```
- `libimobiledevice` and `libplist` allow AFC access to iOS devices.  
- `opencv` provides image processing routines.  
- `flatbuffers` is required by TensorFlow Lite headers.  
- `cmake` and `make` drive the build system.  
- `bazel` is needed to build TensorFlow Lite from source.

### 3. Clone and build TensorFlow Lite

iPurity expects TensorFlow Lite’s C++ headers and `libtensorflowlite.so` (or `.dylib`) built with Bazel. Follow these steps:

1. **Clone TensorFlow** under your project folder (we’ll assume `/Users/agent-hellboy/iPurity/src/tensorflow`):

   ```bash
   cd /Users/agent-hellboy/iPurity/src
   git clone https://github.com/tensorflow/tensorflow.git
   ```

2. **Enter the TensorFlow directory**:

   ```bash
   cd tensorflow
   ```

3. **Configure (optional, most defaults suffice)**:

   ```bash
   ./configure
   ```

   - If prompted about Python, just press Enter (we only need C++ build).  
   - When asked for iOS or ROCm support, answer `N`.

4. **Build TFLite with Bazel**:

   ```bash
   bazel build -c opt //tensorflow/lite:libtensorflowlite.so
   ```

   This produces:

   ```
   /Users/agent-hellboy/iPurity/src/tensorflow/bazel-bin/tensorflow/lite/libtensorflowlite.so
   ```

5. **Note the include directory**:

   ```
   /Users/agent-hellboy/iPurity/src/tensorflow  (root containing `tensorflow/lite/interpreter.h`)
   ```

   and the FlatBuffers headers will be available at:
   ```
   /usr/local/include/flatbuffers   (via Homebrew)
   ```


## Installation

You have two main options: install via our Homebrew tap, or build from source.

### Option 1: Homebrew (Custom Tap)

You can install iPurity from our custom Homebrew tap:

```bash
brew tap Agent-Hellboy/homebrew-agent-hellboy-formula
brew install agent-hellboy/homebrew-agent-hellboy-formula/ipurity
```

This installs the prebuilt binary and handles dependencies automatically.

### Option 2: Build from Source

1. **Ensure all prerequisites are installed** (see above).

2. **Clone iPurity** and enter the project root:

   ```bash
   git clone https://github.com/Agent-Hellboy/iPurity.git
   cd iPurity
   ```

3. **Configure (optional, most defaults suffice)**:

   TODO
   configure to build for macOS so that we avoid passing build flags during CMake configuration.

4. **Create a `build/` directory** and configure with CMake, passing the TensorFlow Lite include and library paths:

   ```bash
   mkdir build && cd build
   cmake      -DTFLITE_INCLUDE_DIR=/Users/agent-hellboy/iPurity/src/tensorflow      -DTFLITE_LIB=/Users/agent-hellboy/iPurity/src/tensorflow/bazel-bin/tensorflow/lite/libtensorflowlite.so      -DCMAKE_BUILD_TYPE=Release      ..
   ```

   - `TFLITE_INCLUDE_DIR` should point to the TensorFlow root that contains `tensorflow/lite/interpreter.h`.  
   - `TFLITE_LIB` should point to the Bazel‐built `libtensorflowlite.so`.  
   - Homebrew’s FlatBuffers are found automatically via `/usr/local/include` or `/opt/homebrew/include`.

5. **Build**:

   ```bash
   cmake --build . --config Release
   ```

6. **Install (optional)**:

   ```bash
   sudo cmake --build . --target install
   ```

   This installs the `ipurity` binary to `/usr/local/bin/ipurity`.

## Usage

From the project root (so that `models/nsfw_model.tflite` is found relative to the binary), run:

```bash
./build/iPurity <threshold>
```

- `<threshold>` is the NSFW confidence threshold (default `0.6` if omitted).
- Example:
  ```bash
  ./build/iPurity 0.7
  ```

If an iPhone/iPad is connected, unlocked, and trusted, iPurity will scan `/DCIM` on the device. Otherwise, it will attempt to scan `/DCIM` on the local filesystem.

## Development

### Scanning a Local Folder Instead of a Device

By default, iPurity tries to open AFC sessions to scan an attached iOS device. If you want to scan a local directory (e.g. a sample “DCIM” of images):

1. Create a local folder and copy some images:
   ```bash
   mkdir -p LocalDCIM
   cp ~/Pictures/*.jpg LocalDCIM/
   ```
2. Edit `src/scanner.cpp` (or `nsfw_detector.cpp`) to replace `"/DCIM"` with `"LocalDCIM"`, then rebuild.

### Build with Thread Sanitizer

To detect data races, build with `-fsanitize=thread`:

```bash
export CFLAGS="-fsanitize=thread"
export LDFLAGS="-fsanitize=thread"
cd build
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread"       -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"       ..
cmake --build . --config Debug
```

---

## Architecture Overview

```
                    +----------------+
                    |     main()     |
                    +----------------+
                            │
                            ▼
            +-------------------------------+
            | Create Device & AFC Client Pool |
            +-------------------------------+
                            │
                            ▼
                    +-----------------+
                    | scan_directory()|
                    +-----------------+
                            │
         ┌──────────────────┼──────────────────┐
         │                                     │
         ▼                                     ▼
[Directory Entry]                      [File Entry]
(recursive call)               (launch async task for file)
                                         │
                                         ▼
                                +---------------------------+
                                | process_image_file()      |
                                +---------------------------+
                                         │
                                         ▼
                           +-------------------------------+
                           |  [Thread X]                   |
                           |  1. Acquire AFC client from   |
                           |     pool                      |
                           |  2. download_file()           |
                           |  3. naiveNSFWCheck()          |
                           |  4. Release client back to pool|
                           |  5. Update ScanStats (mutex)  |
                           +-------------------------------+
```

## License

iPurity is released under the MIT License.
