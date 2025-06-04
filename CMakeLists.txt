cmake_minimum_required(VERSION 3.10)
project(iPurity)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# A) Allow the user to override key include/lib paths on the command line
# -----------------------------------------------------------------------------

# 1) FlatBuffers
if (NOT DEFINED FLATBUFFERS_INCLUDE_DIR)
  set(FLATBUFFERS_INCLUDE_DIR "" CACHE PATH "Path to FlatBuffers include directory")
endif()

# 2) TensorFlow Lite
if (NOT DEFINED TFLITE_INCLUDE_DIR)
  set(TFLITE_INCLUDE_DIR "" CACHE PATH "Path to TensorFlow Lite include directory")
endif()
if (NOT DEFINED TFLITE_LIB)
  set(TFLITE_LIB "" CACHE FILEPATH "Path to TensorFlow Lite library (libtensorflowlite.dylib or .so)")
endif()

# 3) libimobiledevice / libplist
if (NOT DEFINED IMOBILEDEVICE_INCLUDE_DIR)
  set(IMOBILEDEVICE_INCLUDE_DIR "" CACHE PATH "Path to libimobiledevice headers")
endif()
if (NOT DEFINED IMOBILEDEVICE_LIB)
  set(IMOBILEDEVICE_LIB "" CACHE FILEPATH "Path to libimobiledevice library")
endif()
if (NOT DEFINED PLIST_LIB)
  set(PLIST_LIB "" CACHE FILEPATH "Path to libplist library")
endif()

# -----------------------------------------------------------------------------
# 1. Find OpenCV (unchanged)
# -----------------------------------------------------------------------------
find_package(OpenCV REQUIRED)
message(STATUS "→ OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "→ OpenCV libraries:   ${OpenCV_LIBS}")

# -----------------------------------------------------------------------------
# 2. Find TensorFlow Lite (only if not overridden)
# -----------------------------------------------------------------------------
if (NOT TFLITE_INCLUDE_DIR)
  find_path(
    TFLITE_INCLUDE_DIR
    NAMES tensorflow/lite/interpreter.h
    PATHS /usr/local/include /opt/homebrew/include
  )
endif()

if (NOT TFLITE_LIB)
  find_library(
    TFLITE_LIB
    NAMES tensorflow-lite tensorflowlite
    PATHS /usr/local/lib /opt/homebrew/lib
  )
endif()

if (NOT TFLITE_INCLUDE_DIR OR NOT TFLITE_LIB)
  message(FATAL_ERROR
    "TensorFlow Lite not found!\n"
    "Please run ./install.sh to install dependencies, or pass:\n"
    "  -DTFLITE_INCLUDE_DIR=/path/to/tensorflow/lite/includes\n"
    "  -DTFLITE_LIB=/path/to/libtensorflowlite.dylib"
  )
endif()
message(STATUS "→ TFLite include dir: ${TFLITE_INCLUDE_DIR}")
message(STATUS "→ TFLite library:    ${TFLITE_LIB}")

# -----------------------------------------------------------------------------
# 3. Find FlatBuffers (only if FLATBUFFERS_INCLUDE_DIR is empty)
# -----------------------------------------------------------------------------
if (NOT FLATBUFFERS_INCLUDE_DIR)
  find_path(
    FLATBUFFERS_INCLUDE_DIR
    NAMES flatbuffers/flatbuffers.h
    PATHS /usr/local/include /opt/homebrew/include
  )
  if (NOT FLATBUFFERS_INCLUDE_DIR)
    message(FATAL_ERROR
      "FlatBuffers headers not found!\n"
      "Please run ./install.sh to install dependencies, or pass:\n"
      "  -DFLATBUFFERS_INCLUDE_DIR=/path/to/flatbuffers/includes"
    )
  endif()
  message(STATUS "→ Using system FlatBuffers headers: ${FLATBUFFERS_INCLUDE_DIR}")
else()
  message(STATUS "→ Using user-provided FlatBuffers headers: ${FLATBUFFERS_INCLUDE_DIR}")
endif()

# -----------------------------------------------------------------------------
# 4. Find libimobiledevice and libplist
# -----------------------------------------------------------------------------
# If the user did not pass any of these on the command line, try to locate.
if (NOT IMOBILEDEVICE_INCLUDE_DIR)
  find_path(
    IMOBILEDEVICE_INCLUDE_DIR
    NAMES libimobiledevice/afc.h
    PATHS /usr/local/include /opt/homebrew/include
  )
endif()

if (NOT IMOBILEDEVICE_LIB)
  find_library(
    IMOBILEDEVICE_LIB
    NAMES imobiledevice
    PATHS /usr/local/lib /opt/homebrew/lib
  )
endif()

if (NOT PLIST_LIB)
  find_library(
    PLIST_LIB
    NAMES plist
    PATHS /usr/local/lib /opt/homebrew/lib
  )
endif()

if (NOT IMOBILEDEVICE_INCLUDE_DIR OR NOT IMOBILEDEVICE_LIB OR NOT PLIST_LIB)
  message(FATAL_ERROR
    "libimobiledevice or libplist not found!\n"
    " • Install via Homebrew: brew install libimobiledevice libplist\n"
    " • Or specify paths explicitly when invoking CMake:\n"
    "     -DIMOBILEDEVICE_INCLUDE_DIR=/path/to/libimobiledevice/includes \\\n"
    "     -DIMOBILEDEVICE_LIB=/path/to/libimobiledevice.dylib \\\n"
    "     -DPLIST_LIB=/path/to/libplist.dylib"
  )
endif()

message(STATUS "→ libimobiledevice include dir: ${IMOBILEDEVICE_INCLUDE_DIR}")
message(STATUS "→ libimobiledevice lib:         ${IMOBILEDEVICE_LIB}")
message(STATUS "→ libplist lib:                 ${PLIST_LIB}")

# -----------------------------------------------------------------------------
# 5. Define the iPurity executable
# -----------------------------------------------------------------------------
add_executable(iPurity
    src/main.cpp
    src/scanner.cpp
    src/nsfw_detector.cpp
    src/afc_client_pool.cpp
    src/afc_helpers.cpp
)

# -----------------------------------------------------------------------------
# 6. Set include directories (in the order we want)
# -----------------------------------------------------------------------------
target_include_directories(iPurity PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${TFLITE_INCLUDE_DIR}
    ${FLATBUFFERS_INCLUDE_DIR}
    ${IMOBILEDEVICE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -----------------------------------------------------------------------------
# 7. Compiler warnings
# -----------------------------------------------------------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(iPurity PRIVATE -Wall -Wextra)
endif()

# -----------------------------------------------------------------------------
# 8. Link libraries
# -----------------------------------------------------------------------------
target_link_libraries(iPurity PRIVATE
    ${OpenCV_LIBS}
    ${TFLITE_LIB}
    ${IMOBILEDEVICE_LIB}
    ${PLIST_LIB}
    "-framework Accelerate"   # TFLite on macOS needs Accelerate
)

# -----------------------------------------------------------------------------
# 9. Custom targets (configure, check, dist, install)
# -----------------------------------------------------------------------------
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
