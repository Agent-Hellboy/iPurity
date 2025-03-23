cmake_minimum_required(VERSION 3.10)
project(ipurity)

# Use C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(
  ${PROJECT_SOURCE_DIR}/include
  /opt/homebrew/opt/opencv/include/opencv4
  /opt/homebrew/include
)

# Find OpenCV via pkg-config (try opencv4, then fallback to opencv)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENCV opencv4)
if(NOT OPENCV_FOUND)
  pkg_check_modules(OPENCV opencv)
endif()
if(NOT OPENCV_FOUND)
  message(FATAL_ERROR "OpenCV not found!")
endif()

include_directories(${OPENCV_INCLUDE_DIRS})
link_directories(${OPENCV_LIBRARY_DIRS})

# Set additional libraries (iMobileDevice)
set(IMOBILEDEVICE_LIBS "-limobiledevice -lplist")
link_directories(/opt/homebrew/lib)

# Define the executable and its source files
add_executable(ipurity
    src/afc_scanner.cpp
    src/nsfw_detector.cpp
)

# Set compiler warnings
target_compile_options(ipurity PRIVATE -Wall -Wextra)

# Link libraries
target_link_libraries(ipurity ${OPENCV_LIBRARIES} ${IMOBILEDEVICE_LIBS})

#------------------------------------------------------------------------------
# Custom targets to mimic Makefile phony targets for CI pipeline
#------------------------------------------------------------------------------

# Dummy "configure" target
add_custom_target(configure
    COMMAND ${CMAKE_COMMAND} -E echo "Running dummy configure script..."
    COMMAND ${CMAKE_COMMAND} -E echo "All dependencies are assumed to be in place for Apple Silicon."
)

# Dummy "check" target
add_custom_target(check
    COMMAND ${CMAKE_COMMAND} -E echo "Running 'make check' (no tests implemented)."
)

# "dist" target: creates a tarball of the distribution
add_custom_target(dist
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/dist
    COMMAND tar -czf ${PROJECT_SOURCE_DIR}/dist/ipurity.tar.gz
            README.md configure Makefile include src
            --exclude='*.o' --exclude='dist' --exclude='*.tar.gz'
    COMMENT "Creating distribution tarball..."
)

# "distcheck" target: depends on dist and echoes dummy messages
add_custom_target(distcheck
    DEPENDS dist
    COMMAND ${CMAKE_COMMAND} -E echo "Running distcheck (dummy)."
    COMMAND ${CMAKE_COMMAND} -E echo "Would normally build and test from the unpacked tarball."
)

# Installation
install(TARGETS ipurity
  RUNTIME DESTINATION bin
)

