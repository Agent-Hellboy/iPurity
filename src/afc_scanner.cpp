#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>      
#include <chrono>
#include <iomanip>

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

// Structure to hold scan statistics
struct ScanStats {
    int totalFiles = 0;
    int nsfwFiles  = 0;
    int safeFiles  = 0;
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
 * Example function to download a file from iPhone (via AFC)
 * and save it locally. (Stub - not fully robust.)
 *
 * Returns true on success, false on error.
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

    while (true) {
        afc_error_t readErr = afc_file_read(afc, fileRef, buffer, BUF_SIZE, &bytesRead);
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
 * Recursively list files and directories on the iPhone via AFC.
 * For each image file, download the file and run the NSFW check.
 * Statistics are updated in the ScanStats structure.
 */
static void scan_directory(afc_client_t afc, const char *path, ScanStats &stats) {
    char **dirList = NULL;
    afc_error_t err = afc_read_directory(afc, path, &dirList);
    if (err != AFC_E_SUCCESS) {
        std::cerr << "Error reading directory " << path
                  << " (afc error " << err << ")" << std::endl;
        return;
    }

    for (int i = 0; dirList[i]; i++) {
        const char *entry = dirList[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) {
            continue;
        }

        char *fullPath = nullptr;
        asprintf(&fullPath, "%s/%s", path, entry);

        char **fileInfo = NULL;
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

            if (isDir) {
                scan_directory(afc, fullPath, stats);
            } else {
                std::string filePathStr(fullPath);
                if (is_image_file(filePathStr)) {
                    stats.totalFiles++;
                    std::cout << "Found image file: " << fullPath << std::endl;
                    std::string localFile = "/tmp/ios_" + filePathStr.substr(filePathStr.find_last_of("/") + 1);
                    if (download_file(afc, fullPath, localFile.c_str())) {
                        bool isNSFW = naiveNSFWCheck(localFile);
                        if (isNSFW) {
                            stats.nsfwFiles++;
                            stats.nsfwFilesList.push_back(localFile);
                            std::cout << COLOR_RED << "[NSFW DETECTED] " << localFile << COLOR_RESET << std::endl;
                        } else {
                            stats.safeFiles++;
                            std::cout << COLOR_GREEN << "[SAFE] " << localFile << COLOR_RESET << std::endl;
                        }
                        // Optionally remove the local file if you don't need it:
                        // remove(localFile.c_str());
                    }
                }
            }
        }
        free(fullPath);
    }
    afc_dictionary_free(dirList);
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

    // Start timer for the scan
    auto startTime = std::chrono::high_resolution_clock::now();

    const char *rootPath = "/DCIM";
    std::cout << "Scanning directory: " << rootPath << std::endl;

    ScanStats stats;
    scan_directory(afc, rootPath, stats);

    // End timer for the scan
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    
    // Cleanup
    afc_client_free(afc);
    lockdownd_client_free(client);
    idevice_free(device);

    // Print final report in tabular format
    std::cout << "\n------------------- Scan Report -------------------\n";
    std::cout << std::left << std::setw(35) << "Total Image Files Scanned:" << stats.totalFiles << "\n";
    std::cout << std::left << std::setw(35) << "NSFW Files Detected:" << stats.nsfwFiles << "\n";
    std::cout << std::left << std::setw(35) << "Safe Files Detected:" << stats.safeFiles << "\n";
    std::cout << std::left << std::setw(35) << "Time Taken minutes:" << elapsed.count() / 60 << "\n";
    std::cout << "NSFW Files List:" << std::endl;
    for (const auto &file : stats.nsfwFilesList) {
        std::cout << file << std::endl;
    }
    std::cout << "-----------------------------------------------------\n";
    return 0;
}
