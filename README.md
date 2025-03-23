# iPurity

[![Build](https://github.com/Agent-Hellboy/iPurity/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/Agent-Hellboy/iPurity/actions/workflows/c-cpp.yml)

iPurity is a simple NSFW (Not Safe For Work) detector for iOS devices.

It utilizes AFC (Apple File Conduit) to list and open each media file and OpenCV to detect nsfw images. 

Note: Tested only on Apple Silicon Mac

## Why did I make this?
- The program was created because one of my younger siblings accidentally encountered NSFW content.
- There is often no time to manually check all images on their phone before passing it to a younger sibling.
- This program aims to assist in identifying potentially inappropriate content.

## Disclaimer
- There may be many false negatives; however, it can help reduce the dataset to scan by approximately 90-95%.

## Prerequisites

- libimobiledevice
- OpenCV    
- Make 

## Installation

### Option 1: Homebrew (Broken)

You can install iPurity from your custom Homebrew tap:

```bash
brew tap Agent-Hellboy/homebrew-agent-hellboy-formula
brew install ipurity
```

### Option 2: Build from Source
```bash
./configure
mkdir build
cd build
cmake ..
cmake --build .
```

### Installation
```bash
sudo cmake --build . --target install
```

## Usage

```bash 
./ipurity
```

## License  

iPurity is released under the MIT License.
