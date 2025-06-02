#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <mutex>
#include <vector>

#include "scanner.h"
#include "afc_helpers.h"
#include "nsfw_detector.h"
#include <opencv2/opencv.hpp>

// External global objects (could also be placed in a dedicated logging module)
extern std::mutex coutMutex;
extern std::mutex statsMutex;
extern std::vector<std::future<void>> futures;

// ANSI escape codes for colors
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_RED = "\033[31m";
const char* COLOR_RESET = "\033[0m";

bool download_file_to_buffer(afc_client_t afc, const char* remotePath, std::vector<uchar>& buffer) {
    uint64_t fileRef = 0;
    if (afc_file_open(afc, remotePath, AFC_FOPEN_RDONLY, &fileRef) != AFC_E_SUCCESS) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "Failed to open remote file: " << remotePath << std::endl;
        return false;
    }
    const size_t BUF_SIZE = 4096;
    std::vector<uchar> temp(BUF_SIZE);
    uint32_t bytesRead = 0;
    while (true) {
        afc_error_t readErr = afc_file_read(afc, fileRef, temp.data(), BUF_SIZE, &bytesRead);
        if (readErr != AFC_E_SUCCESS || bytesRead == 0) break;
        buffer.insert(buffer.end(), temp.begin(), temp.begin() + bytesRead);
    }
    afc_file_close(afc, fileRef);
    return !buffer.empty();
}

void process_image_file(AfcClientPool* pool, const char* fullPath,
                        ScanStats& stats, float threshold, NSFWDetector* detector) {
    std::string filePathStr(fullPath);
    if (!is_image_file(filePathStr)) return;

    // Update stats.
    {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.totalFiles++;
    }
    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "Found image file: " << fullPath << std::endl;
    }

    afc_client_t client = pool->acquire();
    std::vector<uchar> buffer;
    if (download_file_to_buffer(client, fullPath, buffer)) {
        cv::Mat image = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (!image.empty()) {
            float nsfwProbability = detector->detectNSFW(image);
            bool isNSFW = nsfwProbability >= threshold;
            std::string message;
            {
                std::lock_guard<std::mutex> lock(statsMutex);
                if (isNSFW) {
                    stats.nsfwFiles++;
                    stats.nsfwFilesList.push_back(filePathStr);
                    message = std::string(COLOR_RED) + "[NSFW DETECTED] " +
                              filePathStr + " (Probability: " + 
                              std::to_string(nsfwProbability) + ")" + COLOR_RESET;
                } else {
                    stats.safeFiles++;
                    message = std::string(COLOR_GREEN) + "[SAFE] " + filePathStr +
                              " (Probability: " + std::to_string(nsfwProbability) + 
                              ")" + COLOR_RESET;
                }
            }
            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << message << std::endl;
            }
        }
    }
    pool->release(client);
}

void scan_directory(AfcClientPool* pool, const char* path, ScanStats& stats,
                    float threshold, NSFWDetector* detector) {
    afc_client_t client = pool->acquire();
    char** dirList = nullptr;
    afc_error_t err = afc_read_directory(client, path, &dirList);
    pool->release(client);

    if (err != AFC_E_SUCCESS) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "Error reading directory " << path << " (afc error " << err
                  << ")" << std::endl;
        return;
    }

    for (int i = 0; dirList[i]; i++) {
        const char* entry = dirList[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;

        char* fullPath = build_full_path(path, entry);

        client = pool->acquire();
        bool isDir = is_directory(client, fullPath);
        pool->release(client);

        if (isDir) {
            scan_directory(pool, fullPath, stats, threshold, detector);
        } else {
            futures.push_back(std::async(std::launch::async, process_image_file,
                                         pool, fullPath, std::ref(stats),
                                         threshold, detector));
        }
        free(fullPath);
    }
    afc_dictionary_free(dirList);
}
