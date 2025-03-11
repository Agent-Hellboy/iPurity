CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra \
    -Iinclude \
    -I/opt/homebrew/opt/opencv/include/opencv4 \
    -I/opt/homebrew/include

OPENCV_LIBS = `pkg-config --libs opencv4 2>/dev/null || pkg-config --libs opencv`
IMOBILEDEVICE_LIBS = -L/opt/homebrew/lib -limobiledevice -lplist

TARGET = ios_nsfw_scanner
OBJS = src/afc_scanner.o src/nsfw_detector.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(OPENCV_LIBS) $(IMOBILEDEVICE_LIBS)

# Compile afc_scanner.cpp
src/afc_scanner.o: src/afc_scanner.cpp
	$(CXX) $(CXXFLAGS) -c src/afc_scanner.cpp -o $@

# Compile nsfw_detector.cpp
# (Adjust if your naiveNSFWCheck is in a different file)
src/nsfw_detector.o: src/nsfw_detector.cpp include/nsfw_detector.h
	$(CXX) $(CXXFLAGS) -c src/nsfw_detector.cpp -o $@

clean:
	rm -f src/*.o $(TARGET)
