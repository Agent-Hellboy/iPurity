cmake_minimum_required(VERSION 3.10)
project(iPurity)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------
# A) Allow FLATBUFFERS_INCLUDE_DIR to be passed on the command line
# ----------------------------------------------------------------
if (NOT DEFINED FLATBUFFERS_INCLUDE_DIR)
  set(FLATBUFFERS_INCLUDE_DIR "" CACHE PATH "Path to FlatBuffers include directory")
endif()

# ----------------------------------------------------------------
# 1. Find OpenCV
# ----------------------------------------------------------------
find_package(OpenCV REQUIRED)
message(STATUS "→ OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "→ OpenCV libraries:   ${OpenCV_LIBS}")

# ----------------------------------------------------------------
# 2. Find TensorFlow Lite
# ----------------------------------------------------------------
find_path(
    TFLITE_INCLUDE_DIR
    NAMES tensorflow/lite/interpreter.h
    PATHS
      ${PROJECT_SOURCE_DIR}/src/tensorflow
      /usr/local/include
      /opt/homebrew/include
      NO_DEFAULT_PATH
)
find_library(
    TFLITE_LIB
    NAMES tensorflow-lite tensorflowlite
    PATHS
      ${PROJECT_SOURCE_DIR}/src/tensorflow/bazel-bin/tensorflow/lite
      /usr/local/lib
      /opt/homebrew/lib
      NO_DEFAULT_PATH
)

if (NOT TFLITE_INCLUDE_DIR OR NOT TFLITE_LIB)
    message(FATAL_ERROR
        "TensorFlow Lite not found!\n"
        " • Build TFLite via Bazel:\n"
        "     cd ${PROJECT_SOURCE_DIR}/src/tensorflow\n"
        "     bazel build -c opt //tensorflow/lite:libtensorflowlite.so\n"
        " • Then rerun CMake from project root with:\n"
        "     -DTFLITE_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/src/tensorflow \\\n"
        "     -DTFLITE_LIB=${PROJECT_SOURCE_DIR}/src/tensorflow/bazel-bin/tensorflow/lite/libtensorflowlite.so \\\n"
        "     -DCMAKE_BUILD_TYPE=Release"
    )
endif()
message(STATUS "→ TFLite include dir: ${TFLITE_INCLUDE_DIR}")
message(STATUS "→ TFLite library:    ${TFLITE_LIB}")


# ----------------------------------------------------------------
# 3. Find FlatBuffers – prefer Bazel’s copy (so it matches TFLite v24)
# ----------------------------------------------------------------
# Bazel always places the FlatBuffers v24 headers under:
#   <TF-SRC>/bazel-bin/external/flatbuffers/include/flatbuffers/flatbuffers.h
#
# Assume CMakeLists.txt is in project root, and TFLite was built via Bazel
set(FLAT_CANDIDATE1
    "${CMAKE_SOURCE_DIR}/src/tensorflow/bazel-bin/external/flatbuffers/include/flatbuffers/flatbuffers.h"
    )
if (EXISTS "${FLAT_CANDIDATE1}")
    # Remove “/flatbuffers/flatbuffers.h” to point FLATBUFFERS_INCLUDE_DIR at “…/include”
    get_filename_component(FLATBUFFERS_INCLUDE_DIR "${FLAT_CANDIDATE1}" DIRECTORY)
    message(STATUS "→ Using Bazel’s FlatBuffers headers: ${FLATBUFFERS_INCLUDE_DIR}")
else()
    # Fallback: check for Homebrew‐installed FlatBuffers v24 (Intel or Apple Silicon)
    find_path(
        FLATBUFFERS_INCLUDE_DIR
        NAMES flatbuffers/flatbuffers.h
        PATHS /usr/local/include /opt/homebrew/include
        NO_DEFAULT_PATH
    )
    if (NOT FLATBUFFERS_INCLUDE_DIR)
        message(FATAL_ERROR
            "FlatBuffers headers not found!\n"
            "  • Bazel’s copy (expected at ${CMAKE_SOURCE_DIR}/src/tensorflow/bazel-bin/external/flatbuffers/include/flatbuffers/flatbuffers.h) is missing.\n"
            "  • Or Homebrew copy not found. Please run:\n"
            "      brew install flatbuffers@24\n"
            "    (or copy Bazel’s FlatBuffers into /usr/local/include/flatbuffers)."
        )
    endif()
    message(STATUS "→ Using system FlatBuffers headers: ${FLATBUFFERS_INCLUDE_DIR}")
endif()
# ----------------------------------------------------------------
# 4. Find libimobiledevice and libplist
# ----------------------------------------------------------------
find_path(
    IMOBILEDEVICE_INCLUDE_DIR
    NAMES libimobiledevice/libimobiledevice.h
    PATHS /usr/local/include /opt/homebrew/include NO_DEFAULT_PATH
)
find_library(
    IMOBILEDEVICE_LIB
    NAMES imobiledevice
    PATHS /usr/local/lib /opt/homebrew/lib NO_DEFAULT_PATH
)
find_library(
    PLIST_LIB
    NAMES plist
    PATHS /usr/local/lib /opt/homebrew/lib NO_DEFAULT_PATH
)

if (NOT IMOBILEDEVICE_INCLUDE_DIR OR NOT IMOBILEDEVICE_LIB OR NOT PLIST_LIB)
    message(FATAL_ERROR
        "libimobiledevice or libplist not found!\n"
        " • Install via Homebrew: brew install libimobiledevice libplist"
    )
endif()
message(STATUS "→ libimobiledevice include dir: ${IMOBILEDEVICE_INCLUDE_DIR}")
message(STATUS "→ libimobiledevice lib:         ${IMOBILEDEVICE_LIB}")
message(STATUS "→ libplist lib:                 ${PLIST_LIB}")

# ----------------------------------------------------------------
# 5. Define the iPurity executable
# ----------------------------------------------------------------
add_executable(iPurity
    src/main.cpp
    src/scanner.cpp
    src/nsfw_detector.cpp
    src/afc_client_pool.cpp
    src/afc_helpers.cpp
)

# ----------------------------------------------------------------
# 6. Set include directories (in this order)
# ----------------------------------------------------------------
target_include_directories(iPurity PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${TFLITE_INCLUDE_DIR}
    ${FLATBUFFERS_INCLUDE_DIR}
    ${IMOBILEDEVICE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ----------------------------------------------------------------
# 7. Compiler warnings
# ----------------------------------------------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(iPurity PRIVATE -Wall -Wextra)
endif()

# ----------------------------------------------------------------
# 8. Link libraries
# ----------------------------------------------------------------
target_link_libraries(iPurity PRIVATE
    ${OpenCV_LIBS}
    ${TFLITE_LIB}
    ${IMOBILEDEVICE_LIB}
    ${PLIST_LIB}
    "-framework Accelerate"   # TFLite on macOS needs Accelerate
)

# ----------------------------------------------------------------
# 9. Custom targets (configure, check, dist, install)
# ----------------------------------------------------------------
add_custom_target(configure
    COMMAND ${CMAKE_COMMAND} -E echo "Ensure all dependencies are installed."
)
add_custom_target(check
    COMMAND ${CMAKE_COMMAND} -E echo "No tests implemented."
)
add_custom_target(dist
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_SOURCE_DIR} /usr/bin/tar -czf dist/iPurity.tar.gz CMakeLists.txt LICENSE README.md configure include src
    COMMENT "Creating distribution tarball..."
)
install(TARGETS iPurity RUNTIME DESTINATION bin)
