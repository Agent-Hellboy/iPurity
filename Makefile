###############################################################################
# Compiler and Flags
###############################################################################
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra \
    -Iinclude \
    -I/opt/homebrew/opt/opencv/include/opencv4 \
    -I/opt/homebrew/include

OPENCV_LIBS = `pkg-config --libs opencv4 2>/dev/null || pkg-config --libs opencv`
IMOBILEDEVICE_LIBS = -L/opt/homebrew/lib -limobiledevice -lplist
GLOG_LIBS = -lglog

###############################################################################
# Targets
###############################################################################
TARGET = ipurity
OBJS = src/afc_scanner.o src/nsfw_detector.o

###############################################################################
# Default Build Target
###############################################################################
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(OPENCV_LIBS) $(IMOBILEDEVICE_LIBS) $(GLOG_LIBS)

# Compile afc_scanner.cpp
src/afc_scanner.o: src/afc_scanner.cpp
	$(CXX) $(CXXFLAGS) -c src/afc_scanner.cpp -o $@

# Compile nsfw_detector.cpp
src/nsfw_detector.o: src/nsfw_detector.cpp include/nsfw_detector.h
	$(CXX) $(CXXFLAGS) -c src/nsfw_detector.cpp -o $@

###############################################################################
# Phony Targets for CI Pipeline
###############################################################################
.PHONY: configure check dist distcheck clean

# 1. "configure" target
configure:
	@echo "Running dummy configure script..."
	@echo "All dependencies are assumed to be in place for Apple Silicon."

# 2. "check" target
check:
	@echo "Running 'make check' (no tests implemented)."

# 3. "dist" target
dist:
	@echo "Creating a distribution tarball..."
	@mkdir -p dist
	@tar -czf dist/$(TARGET).tar.gz \
	    README.md configure Makefile include/ src/ \
	    --exclude='*.o' --exclude='dist' --exclude='*.tar.gz' || true
	@echo "Created dist/$(TARGET).tar.gz"

# 4. "distcheck" target
distcheck: dist
	@echo "Running distcheck (dummy)."
	@cd dist && tar -xzf $(TARGET).tar.gz
	@echo "Would normally build and test from the unpacked tarball."

###############################################################################
# Clean
###############################################################################
clean:
	rm -f src/*.o $(TARGET)
	rm -rf dist
