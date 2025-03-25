#include <libimobiledevice/afc.h>
#include <libimobiledevice/lockdown.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "afc_helpers.h"
#include "nsfw_detector.h"

// ANSI escape codes for colors
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_RED = "\033[31m";
const char* COLOR_RESET = "\033[0m";

struct ScanStats {
    int totalFiles = 0;
    int nsfwFiles = 0;
    int safeFiles = 0;
    std::vector<std::string> nsfwFilesList;
};

std::vector<std::future<void>> futures;
std::mutex statsMutex;

/**
 * A simple thread-safe pool for AFC clients.
 */
class AfcClientPool {
   public:
    // Constructor: creates a pool of 'poolSize' AFC clients for the given
    // device.
    AfcClientPool(idevice_t device, int poolSize) : device_(device) {
        for (int i = 0; i < poolSize; i++) {
            afc_client_t client = nullptr;
            // Start the AFC service for each client.
            if (afc_client_start_service(device_, &client, "afc_scanner") ==
                AFC_E_SUCCESS) {
                pool_.push_back(client);
            } else {
                std::cerr << "Failed to create AFC client " << i << std::endl;
            }
        }
    }

    ~AfcClientPool() {
        for (auto client : pool_) {
            afc_client_free(client);
        }
    }

    // Acquire a client from the pool (blocks until one is available)
    afc_client_t acquire() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !pool_.empty(); });
        afc_client_t client = pool_.back();
        pool_.pop_back();
        return client;
    }

    // Release a client back to the pool.
    void release(afc_client_t client) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            pool_.push_back(client);
        }
        cv_.notify_one();
    }

   private:
    idevice_t device_;
    std::vector<afc_client_t> pool_;
    std::mutex mtx_;
    std::condition_variable cv_;
};

/**
 * Example function to download a file from iOS (via AFC)
 * and save it locally.
 *
 * Returns true on success, false on error.
 */
static bool download_file(afc_client_t afc, const char* remotePath,
                          const char* localPath) {
    uint64_t fileRef = 0;
    if (afc_file_open(afc, remotePath, AFC_FOPEN_RDONLY, &fileRef) !=
        AFC_E_SUCCESS) {
        std::cerr << "Failed to open remote file: " << remotePath << std::endl;
        return false;
    }

    FILE* outFile = std::fopen(localPath, "wb");
    if (!outFile) {
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
        if (readErr != AFC_E_SUCCESS || bytesRead == 0) {
            break;
        }
        std::fwrite(buffer, 1, bytesRead, outFile);
    }

    afc_file_close(afc, fileRef);
    std::fclose(outFile);
    return true;
}

/**
 * Process an image file asynchronously:
 * 1. Acquire an AFC client from the pool.
 * 2. Download the image file.
 * 3. Run an NSFW check.
 * 4. Update shared statistics.
 * 5. Release the AFC client.
 */
static void process_image_file(AfcClientPool* pool, const char* fullPath,
                               ScanStats& stats, float threshold) {
    std::string filePathStr(fullPath);
    if (!is_image_file(filePathStr)) return;

    {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.totalFiles++;
    }

    std::cout << "Found image file: " << fullPath << std::endl;
    std::string localFile =
        "/tmp/ios_" + filePathStr.substr(filePathStr.find_last_of("/") + 1);

    // Acquire an AFC client from the pool.
    afc_client_t client = pool->acquire();
    if (download_file(client, fullPath, localFile.c_str())) {
        bool isNSFW = naiveNSFWCheck(localFile, threshold);
        {
            std::lock_guard<std::mutex> lock(statsMutex);
            if (isNSFW) {
                stats.nsfwFiles++;
                stats.nsfwFilesList.push_back(localFile);
                std::cout << COLOR_RED << "[NSFW DETECTED] " << localFile
                          << COLOR_RESET << std::endl;
            } else {
                stats.safeFiles++;
                std::cout << COLOR_GREEN << "[SAFE] " << localFile
                          << COLOR_RESET << std::endl;
            }
        }
        // Optionally remove the local file if you don't need it:
        // remove(localFile.c_str());
    }
    pool->release(client);
}

/**
 * Recursively scan a directory.
 * For each file (non-directory), launch an asynchronous task to process it.
 */
static void scan_directory(AfcClientPool* pool, const char* path,
                           ScanStats& stats, float threshold) {
    // Acquire an AFC client to read the directory.
    afc_client_t client = pool->acquire();
    char** dirList = nullptr;
    afc_error_t err = afc_read_directory(client, path, &dirList);
    pool->release(client);

    if (err != AFC_E_SUCCESS) {
        std::cerr << "Error reading directory " << path << " (afc error " << err
                  << ")" << std::endl;
        return;
    }

    for (int i = 0; dirList[i]; i++) {
        const char* entry = dirList[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;

        char* fullPath = build_full_path(path, entry);

        // Use an AFC client from the pool to check if this entry is a
        // directory.
        client = pool->acquire();
        bool isDir = is_directory(client, fullPath);
        pool->release(client);

        if (isDir) {
            scan_directory(pool, fullPath, stats, threshold);
        } else {
            // Launch image processing asynchronously.
            futures.push_back(std::async(std::launch::async, process_image_file,
                                         pool, fullPath, std::ref(stats),
                                         threshold));
        }
        free(fullPath);
    }
    afc_dictionary_free(dirList);
}

/**
 * Main entry point.
 */
int main(int argc, char* argv[]) {
    std::cout << "iPurity - NSFW Scanner" << std::endl;
    std::cout << "----------------------" << std::endl;
    float threshold = DEFAULT_SKIN_THRESHOLD;
    if (argc > 1) {
        threshold = std::stof(argv[1]);
        if (threshold < 0.0 || threshold > 1.0) {
            std::cerr << "Threshold must be between 0.0 and 1.0" << std::endl;
            return 1;
        }
    }

    idevice_t device = nullptr;
    if (idevice_new(&device, nullptr) != IDEVICE_E_SUCCESS) {
        std::cerr << "No iOS device found. Is it plugged in and trusted?"
                  << std::endl;
        return 1;
    }

    int poolSize = 4;
    AfcClientPool clientPool(device, poolSize);

    auto startTime = std::chrono::high_resolution_clock::now();

    const char* rootPath = "/DCIM";
    std::cout << "Scanning directory: " << rootPath << std::endl;

    ScanStats stats;
    scan_directory(&clientPool, rootPath, stats, threshold);

    // Wait for all asynchronous tasks to complete.
    for (auto& fut : futures) {
        fut.get();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;

    // Cleanup device connection.
    idevice_free(device);

    std::cout << "\n------------------- Scan Report -------------------\n";
    std::cout << std::left << std::setw(35)
              << "Total Image Files Scanned:" << stats.totalFiles << "\n";
    std::cout << std::left << std::setw(35)
              << "NSFW Files Detected:" << stats.nsfwFiles << "\n";
    std::cout << std::left << std::setw(35)
              << "Safe Files Detected:" << stats.safeFiles << "\n";
    std::cout << std::left << std::setw(35)
              << "Time Taken minutes:" << elapsed.count() / 60 << "\n";
    std::cout << "NSFW Files List:" << std::endl;
    for (const auto& file : stats.nsfwFilesList) {
        std::cout << file << std::endl;
    }
    std::cout << "-----------------------------------------------------\n";

    return 0;
}
