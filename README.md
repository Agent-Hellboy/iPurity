# iPurity

iPurity is a simple NSFW (Not Safe For Work) detector for iOS devices.

It utilizes AFC (Apple File Conduit) to list and open each media file. No one has the time to manually check all the images on their phone before handing it over to a younger sibling. Simply connect your phone via USB, grant trust to your device, and run the program to scan and delete unwanted content before passing it on.

Note: Tested only on apple silicon mac

## Prerequisites

- libimobiledevice
- OpenCV    
- CMake
- lplist

## Installation

```bash
make
```

## Usage

```bash 
./ios_nsfw_scanner
```

## License  

iPurity is released under the MIT License.
