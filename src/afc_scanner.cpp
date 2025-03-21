#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>
#include <vector>
#include <future>  // For std::async and std::future

// libimobiledevice
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/afc.h>

// Our naive NSFW detection
#include "nsfw_detector.h"

// ANSI escape codes for colors
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_RED   = "\033[31m";
const char* COLOR_RESET = "\033[0m";

// Global mutex for protecting shared state and std::cout:
static std::mutex g_statsMutex;

// Structure to hold scan statistics
struct ScanStats {
    int totalFiles = 0;
    int nsfwFiles  = 0;
    int safeFiles  = 0;
    int errorFiles = 0;
    std::vector<std::string> nsfwFilesList;
};

// Check if the file extension indicates an image file.
bool is_image_file(const std::string &filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) return false;
    
    std::string ext = filePath.substr(dotPos);
    // Convert extension to lowercase
    for (auto &c : ext) c = tolower(c);
    
    return (ext == ".jpg" || ext == ".jpeg" ||
            ext == ".png" || ext == ".gif"  ||
            ext == ".bmp" || ext == ".tiff" ||
            ext == ".webp"); // Add other extensions as needed.
}

/**
 * Download a file from iPhone (via AFC)
 * and save it locally. Returns true on success, false on error.
 */
static bool download_file(afc_client_t afc, const char* remotePath, const char* localPath) {
    uint64_t fileRef = 0;
    if (afc_file_open(afc, remotePath, AFC_FOPEN_RDONLY, &fileRef) != AFC_E_SUCCESS) {
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
    bool success = true;
    
    // Continue reading until end-of-file is reached.
    while (true) {
        afc_error_t readErr = afc_file_read(afc, fileRef, buffer, BUF_SIZE, &bytesRead);
        if (readErr != AFC_E_SUCCESS) {
            std::cerr << "Error reading file " << remotePath << " (afc error " << readErr << ")" << std::endl;
            success = false;
            break;
        }
        if (bytesRead == 0) { // End-of-file reached.
            break;
        }
        size_t bytesWritten = std::fwrite(buffer, 1, bytesRead, outFile);
        if (bytesWritten != bytesRead) {
            std::cerr << "Error writing to local file " << localPath << std::endl;
            success = false;
            break;
        }
    }
    
    afc_file_close(afc, fileRef);
    std::fclose(outFile);
    return success;
}

/**
 * Timeout wrapper for download_file using std::async.
 * If download_file doesn't complete within the specified timeout,
 * a timeout error is logged and false is returned.
 */
static bool download_file_with_timeout(afc_client_t afc, const char* remotePath, const char* localPath, std::chrono::seconds timeout) {
    auto downloadFuture = std::async(std::launch::async, [=]() {
        return download_file(afc, remotePath, localPath);
    });
    
    if (downloadFuture.wait_for(timeout) == std::future_status::timeout) {
        std::cerr << "Timeout downloading file " << remotePath << std::endl;
        return false;
    }
    return downloadFuture.get();
}

/**
 * Recursively list files and directories on the iPhone via AFC.
 * For each image file, download the file and run the NSFW check.
 * Statistics are updated in the ScanStats structure.
 * This multithreaded version launches a new thread for each subdirectory
 * and for each image file processing task.
 */
static void scan_directory(afc_client_t afc, const char *path, ScanStats &stats) {
    char **dirList = nullptr;
    afc_error_t err = afc_read_directory(afc, path, &dirList);
    if (err != AFC_E_SUCCESS) {
        std::cerr << "Error reading directory " << path
                  << " (afc error " << err << ")" << std::endl;
        return;
    }
    
    std::vector<std::thread> threads;
    
    for (int i = 0; dirList[i]; i++) {
        const char *entry = dirList[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) {
            continue;
        }
        
        char *fullPath = nullptr;
        asprintf(&fullPath, "%s/%s", path, entry);
        if (!fullPath)
            continue;
        
        char **fileInfo = nullptr;
        if (afc_get_file_info(afc, fullPath, &fileInfo) == AFC_E_SUCCESS && fileInfo) {
            bool isDir = false;
            for (int j = 0; fileInfo[j]; j += 2) {
                if (strcmp(fileInfo[j], "st_ifmt") == 0) {
                    if (strcmp(fileInfo[j + 1], "S_IFDIR") == 0) {
                        isDir = true;
                    }
                    break;
                }
            }
            afc_dictionary_free(fileInfo);
            
            std::string pathStr(fullPath);
            
            if (isDir) {
                // Spawn a new thread to scan the subdirectory.
                threads.emplace_back([afc, pathStr, &stats]() {
                    scan_directory(afc, pathStr.c_str(), stats);
                });
            } else {
                if (is_image_file(pathStr)) {
                    // Spawn a thread to process the image file.
                    threads.emplace_back([afc, pathStr, &stats]() {
                        {
                            std::lock_guard<std::mutex> lock(g_statsMutex);
                            stats.totalFiles++;
                        }
                        std::cout << "Found image file: " << pathStr << std::endl;
                        // Construct a local file path.
                        std::string localFile = "/tmp/ios_" + pathStr.substr(pathStr.find_last_of("/") + 1);
                        // Use the timeout wrapper (10 seconds timeout) for file download.
                        if (download_file_with_timeout(afc, pathStr.c_str(), localFile.c_str(), std::chrono::seconds(10))) {
                            try {
                                bool isNSFW = naiveNSFWCheck(localFile);
                                {
                                    std::lock_guard<std::mutex> lock(g_statsMutex);
                                    if (isNSFW) {
                                        stats.nsfwFiles++;
                                        stats.nsfwFilesList.push_back(localFile);
                                        std::cout << COLOR_RED << "[NSFW DETECTED] " << localFile << COLOR_RESET << std::endl;
                                    } else {
                                        stats.safeFiles++;
                                        std::cout << COLOR_GREEN << "[SAFE] " << localFile << COLOR_RESET << std::endl;
                                    }
                                }
                            } catch (...) {
                                std::lock_guard<std::mutex> lock(g_statsMutex);
                                stats.errorFiles++;
                                std::cerr << "[ERROR] NSFW scan failed for " << localFile << std::endl;
                            }
                        } else {
                            std::lock_guard<std::mutex> lock(g_statsMutex);
                            stats.errorFiles++;
                        }
                    });
                }
            }
        }
        free(fullPath);
    }
    afc_dictionary_free(dirList);
    
    // Wait for all threads to complete.
    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

/**
 * Main entry point:
 * 1. Connect to the first detected iOS device.
 * 2. Create a lockdownd client.
 * 3. Start the AFC service.
 * 4. Recursively scan a directory (e.g., /DCIM) to find files.
 * 5. Download each image and run naiveNSFWCheck on it.
 * 6. Print a final report of the scan statistics.
 */
int main(int argc, char *argv[]) {
    idevice_t device = NULL;
    lockdownd_client_t client = NULL;
    lockdownd_service_descriptor_t service = NULL;
    afc_client_t afc = NULL;
    
    if (idevice_new(&device, NULL) != IDEVICE_E_SUCCESS) {
        std::cerr << "No iOS device found. Is it plugged in and trusted?" << std::endl;
        return 1;
    }
    
    if (lockdownd_client_new_with_handshake(device, &client, "afc_scanner") != LOCKDOWN_E_SUCCESS) {
        std::cerr << "Could not create lockdownd client." << std::endl;
        idevice_free(device);
        return 1;
    }
    
    if (lockdownd_start_service(client, AFC_SERVICE_NAME, &service) != LOCKDOWN_E_SUCCESS || !service) {
        std::cerr << "Could not start AFC service." << std::endl;
        lockdownd_client_free(client);
        idevice_free(device);
        return 1;
    }
    
    if (afc_client_new(device, service, &afc) != AFC_E_SUCCESS || !afc) {
        std::cerr << "Could not create AFC client." << std::endl;
        lockdownd_service_descriptor_free(service);
        lockdownd_client_free(client);
        idevice_free(device);
        return 1;
    }
    
    lockdownd_service_descriptor_free(service);
    
    // Start timer for the scan.
    auto startTime = std::chrono::high_resolution_clock::now();
    
    const char *rootPath = "/DCIM";
    std::cout << "Scanning directory: " << rootPath << std::endl;
    
    ScanStats stats;
    scan_directory(afc, rootPath, stats);
    
    // End timer for the scan.
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    
    // Cleanup.
    afc_client_free(afc);
    lockdownd_client_free(client);
    idevice_free(device);
    
    // Convert elapsed time to HH:MM:SS.
    int total_seconds = static_cast<int>(elapsed.count());
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    
    // Print final scan report.
    std::cout << "\n------------------- Scan Report -------------------\n";
    std::cout << std::left << std::setw(35) << "Total Image Files Scanned:" << stats.totalFiles << "\n";
    std::cout << std::left << std::setw(35) << "NSFW Files Detected:" << stats.nsfwFiles << "\n";
    std::cout << std::left << std::setw(35) << "Safe Files Detected:" << stats.safeFiles << "\n";
    std::cout << std::left << std::setw(35) << "NSFW Scan Errors:" << stats.errorFiles << "\n";
    std::cout << std::left << std::setw(35) << "Time Taken (hh:mm:ss):" 
              << hours << ":" << std::setw(2) << std::setfill('0') << minutes << ":" 
              << std::setw(2) << seconds << std::setfill(' ') << "\n";
    std::cout << std::left << std::setw(35) << "NSFW Files List:";
    for (const auto& file : stats.nsfwFilesList) {
        std::cout << file << " ";
    }
    std::cout << "\n";
    std::cout << "-----------------------------------------------------\n";
    return 0;
}
