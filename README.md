# iPurity

[![Build](https://github.com/Agent-Hellboy/iPurity/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/Agent-Hellboy/iPurity/actions/workflows/c-cpp.yml)

iPurity is a simple NSFW (Not Safe For Work) detector for iOS devices.

It utilizes AFC (Apple File Conduit) to list and open each media file and OpenCV to detect nsfw images. 

No one has the time to manually check all the images on their phone before handing it over to a younger sibling. Connect your phone via USB, grant trust to your device, and run the program to scan and delete unwanted content before passing it on.

Note: Tested only on Apple Silicon Mac

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
make
```

## Usage

```bash 
./ipurity
```

## License  

iPurity is released under the MIT License.
