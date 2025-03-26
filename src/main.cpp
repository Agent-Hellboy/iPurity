#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "afc_client_pool.h"
#include "nsfw_detector.h"
#include "scanner.h"

// Global mutexes and futures used by scanner.cpp.
std::mutex coutMutex;
std::mutex statsMutex;
std::vector<std::future<void>> futures;

int main(int argc, char* argv[]) {
    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "iPurity - NSFW Scanner" << std::endl;
        std::cout << "----------------------" << std::endl;
    }

    float threshold = 0.5;
    if (argc > 1) {
        threshold = std::stof(argv[1]);
        if (threshold < 0.0 || threshold > 1.0) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "Threshold must be between 0.0 and 1.0" << std::endl;
            return 1;
        }
    }

    idevice_t device = nullptr;
    if (idevice_new(&device, nullptr) != IDEVICE_E_SUCCESS) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "No iOS device found. Is it plugged in and trusted?"
                  << std::endl;
        return 1;
    }

    unsigned int cores = std::thread::hardware_concurrency();
    int poolSize = (cores > 0) ? cores : 4;
    AfcClientPool clientPool(device, poolSize);

    auto startTime = std::chrono::high_resolution_clock::now();

    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "Scanning directory: " << "/DCIM" << std::endl;
    }

    ScanStats stats;
    scan_directory(&clientPool, "/DCIM", stats, threshold);

    // Wait for all asynchronous tasks to complete.
    for (auto& fut : futures) {
        fut.get();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;

    idevice_free(device);

    {
        std::lock_guard<std::mutex> lock(coutMutex);
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
    }

    return 0;
}
