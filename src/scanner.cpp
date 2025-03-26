#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <mutex>

#include "scanner.h"
#include "afc_helpers.h"
#include "nsfw_detector.h"

// External global objects (could also be placed in a dedicated logging module)
extern std::mutex coutMutex;
extern std::mutex statsMutex;
extern std::vector<std::future<void>> futures;

// ANSI escape codes for colors
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_RED = "\033[31m";
const char* COLOR_RESET = "\033[0m";


bool download_file(afc_client_t afc, const char* remotePath,
                   const char* localPath) {
    uint64_t fileRef = 0;
    if (afc_file_open(afc, remotePath, AFC_FOPEN_RDONLY, &fileRef) !=
        AFC_E_SUCCESS) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "Failed to open remote file: " << remotePath << std::endl;
        return false;
    }
    FILE* outFile = std::fopen(localPath, "wb");
    if (!outFile) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "Failed to open local file: " << localPath << std::endl;
        afc_file_close(afc, fileRef);
        return false;
    }
    const size_t BUF_SIZE = 4096;
    char buffer[BUF_SIZE];
    uint32_t bytesRead = 0;
    while (true) {
        afc_error_t readErr =
            afc_file_read(afc, fileRef, buffer, BUF_SIZE, &bytesRead);
        if (readErr != AFC_E_SUCCESS || bytesRead == 0) break;
        size_t bytesWritten = std::fwrite(buffer, 1, bytesRead, outFile);
        if (bytesWritten < bytesRead) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "Failed to write all data to local file." << std::endl;
            std::fclose(outFile);
            afc_file_close(afc, fileRef);
            return false;
        }
    }
    afc_file_close(afc, fileRef);
    std::fclose(outFile);
    return true;
}

void process_image_file(AfcClientPool* pool, const char* fullPath,
                        ScanStats& stats, float threshold) {
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
    std::string localFile =
        "/tmp/ios_" + filePathStr.substr(filePathStr.find_last_of("/") + 1);

    afc_client_t client = pool->acquire();
    if (download_file(client, fullPath, localFile.c_str())) {
        NaiveNsfwScanner scanner;
        bool isNSFW = scanner.scan(localFile, threshold);
        std::string message;
        {
            std::lock_guard<std::mutex> lock(statsMutex);
            if (isNSFW) {
                stats.nsfwFiles++;
                stats.nsfwFilesList.push_back(localFile);
                message = std::string(COLOR_RED) + "[NSFW DETECTED] " +
                          localFile + COLOR_RESET;
            } else {
                stats.safeFiles++;
                message = std::string(COLOR_GREEN) + "[SAFE] " + localFile +
                          COLOR_RESET;
            }
        }
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << message << std::endl;
        }
    }
    pool->release(client);
}

void scan_directory(AfcClientPool* pool, const char* path, ScanStats& stats,
                    float threshold) {
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
            scan_directory(pool, fullPath, stats, threshold);
        } else {
            futures.push_back(std::async(std::launch::async, process_image_file,
                                         pool, fullPath, std::ref(stats),
                                         threshold));
        }
        free(fullPath);
    }
    afc_dictionary_free(dirList);
}
