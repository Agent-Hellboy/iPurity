cmake_minimum_required(VERSION 3.10)
project(iPurity)

# Use C++11 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCV
find_package(OpenCV REQUIRED)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found! Please install OpenCV and set OpenCV_DIR if needed.")
endif()

# Find TensorFlow Lite
find_path(TFLITE_INCLUDE_DIR tensorflow/lite/interpreter.h)
find_library(TFLITE_LIB NAMES tensorflow-lite tensorflowlite)
if(NOT TFLITE_INCLUDE_DIR OR NOT TFLITE_LIB)
    message(FATAL_ERROR "TensorFlow Lite not found! Please install TensorFlow Lite and set TFLITE_INCLUDE_DIR and TFLITE_LIB if needed.")
endif()

# Find libimobiledevice
find_path(IMOBILEDEVICE_INCLUDE_DIR libimobiledevice/libimobiledevice.h)
find_library(IMOBILEDEVICE_LIB NAMES imobiledevice)
find_library(PLIST_LIB NAMES plist)
if(NOT IMOBILEDEVICE_INCLUDE_DIR OR NOT IMOBILEDEVICE_LIB OR NOT PLIST_LIB)
    message(FATAL_ERROR "libimobiledevice or libplist not found! Please install them and set IMOBILEDEVICE_INCLUDE_DIR, IMOBILEDEVICE_LIB, and PLIST_LIB if needed.")
endif()

# Include directories
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${TFLITE_INCLUDE_DIR}
    ${IMOBILEDEVICE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Set additional libraries (iMobileDevice)
set(IMOBILEDEVICE_LIBS "-limobiledevice -lplist")
link_directories(/opt/homebrew/lib)

# Define the executable and its source files
add_executable(iPurity
    src/main.cpp
    src/scanner.cpp
    src/nsfw_detector.cpp
    src/afc_client_pool.cpp
    src/afc_helpers.cpp
)

# Set compiler warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(iPurity PRIVATE -Wall -Wextra)
endif()

# Link libraries
target_link_libraries(iPurity
    ${OpenCV_LIBS}
    ${TFLITE_LIB}
    ${IMOBILEDEVICE_LIB}
    ${PLIST_LIB}
)

#------------------------------------------------------------------------------
# Custom targets to mimic Makefile phony targets for CI pipeline
#------------------------------------------------------------------------------

# Dummy "configure" target
add_custom_target(configure
    COMMAND ${CMAKE_COMMAND} -E echo "Running dummy configure script..."
    COMMAND ${CMAKE_COMMAND} -E echo "All dependencies are assumed to be in place."
)

# Dummy "check" target
add_custom_target(check
    COMMAND ${CMAKE_COMMAND} -E echo "Running 'make check' (no tests implemented)."
)

# "dist" target: creates a tarball of the distribution
add_custom_target(dist
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/dist
  COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_SOURCE_DIR} /usr/bin/tar -czf dist/iPurity.tar.gz CMakeLists.txt LICENSE README.md configure include src
  COMMENT "Creating distribution tarball..."
)

# Installation
install(TARGETS iPurity
  RUNTIME DESTINATION bin
)

