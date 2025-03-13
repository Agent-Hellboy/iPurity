#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <future>
#include <algorithm>

// libimobiledevice
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/afc.h>

// Google Logging
#include <glog/logging.h>

// Our naive NSFW detection (assumed to be thread-safe)
#include "nsfw_detector.h"

const float SKIN_THRESHOLD = 0.3f;

/**
 * Download a file from iPhone (via AFC) and save it locally.
 * Returns true on success, false on error.
 */
static bool download_file(afc_client_t afc, const char* remotePath, const char* localPath) {
    uint64_t fileRef = 0;
    if (afc_file_open(afc, remotePath, AFC_FOPEN_RDONLY, &fileRef) != AFC_E_SUCCESS) {
        LOG(ERROR) << "Failed to open remote file: " << remotePath;
        return false;
    }

    FILE* outFile = std::fopen(localPath, "wb");
    if (!outFile) {
        LOG(ERROR) << "Failed to open local file: " << localPath;
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
 * Recursively scan directories on the iPhone via AFC, processing image files in parallel.
 */
static void scan_directory(afc_client_t afc, const char *path, float skinThreshold) {
    char **dirList = nullptr;
    afc_error_t err = afc_read_directory(afc, path, &dirList);
    if (err != AFC_E_SUCCESS) {
        LOG(ERROR) << "Error reading directory " << path << " (afc error " << err << ")";
        return;
    }

    std::vector<std::future<void>> futures;
    for (int i = 0; dirList[i]; i++) {
        const char *entry = dirList[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0)
            continue;

        char *fullPath = nullptr;
        asprintf(&fullPath, "%s/%s", path, entry);
        std::string fullPathStr(fullPath);  // Copy into std::string for safe capture
        free(fullPath);  // free the original pointer

        char **fileInfo = nullptr;
        if (afc_get_file_info(afc, fullPathStr.c_str(), &fileInfo) == AFC_E_SUCCESS && fileInfo) {
            bool isDir = false;
            for (int j = 0; fileInfo[j]; j += 2) {
                if (strcmp(fileInfo[j], "st_ifmt") == 0) {
                    if (strcmp(fileInfo[j+1], "S_IFDIR") == 0) {
                        isDir = true;
                    }
                    break;
                }
            }
            afc_dictionary_free(fileInfo);

            if (isDir) {
                // Recursively scan subdirectory, passing skinThreshold along
                futures.push_back(std::async(std::launch::async, scan_directory, afc, fullPathStr.c_str(), skinThreshold));
            } else {
                std::string filePathStr(fullPathStr);
                if (filePathStr.size() > 4) {
                    std::string ext = filePathStr.substr(filePathStr.size() - 4);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".jpg" || ext == ".png") {
                        LOG(INFO) << "Found image file: " << filePathStr;
                        // Process file download and NSFW check asynchronously
                        futures.push_back(std::async(std::launch::async, [afc, fullPathStr, skinThreshold]() {
                            const char* base = strrchr(fullPathStr.c_str(), '/');
                            std::string localFile = "/tmp/ios_" + std::string(base ? base + 1 : fullPathStr.c_str());
                            if (download_file(afc, fullPathStr.c_str(), localFile.c_str())) {
                                bool isNSFW = naiveNSFWCheck(localFile, skinThreshold);
                                if (isNSFW) {
                                    LOG(INFO) << "[NSFW DETECTED] " << localFile;
                                } else {
                                    LOG(INFO) << "[SAFE] " << localFile;
                                }
                            } else {
                                LOG(ERROR) << "Failed to download file: " << fullPathStr;
                            }
                        }));
                    }
                }
            }
        }
    }
    afc_dictionary_free(dirList);
    for (auto &f : futures) {
        f.get();
    }
}

int main(int argc, char *argv[]) {
    // Initialize Google Logging
    google::InitGoogleLogging(argv[0]);

    // Parse skin threshold from command line arguments
    float skinThreshold = 0.3f;
    if (argc > 1) {
        skinThreshold = std::stof(argv[1]);
    }

    idevice_t device = nullptr;
    lockdownd_client_t client = nullptr;
    lockdownd_service_descriptor_t service = nullptr;
    afc_client_t afc = nullptr;

    if (idevice_new(&device, nullptr) != IDEVICE_E_SUCCESS) {
        LOG(ERROR) << "No iOS device found. Is it plugged in and trusted?";
        return 1;
    }

    if (lockdownd_client_new_with_handshake(device, &client, "afc_scanner") != LOCKDOWN_E_SUCCESS) {
        LOG(ERROR) << "Could not create lockdownd client.";
        idevice_free(device);
        return 1;
    }

    if (lockdownd_start_service(client, AFC_SERVICE_NAME, &service) != LOCKDOWN_E_SUCCESS || !service) {
        LOG(ERROR) << "Could not start AFC service.";
        lockdownd_client_free(client);
        idevice_free(device);
        return 1;
    }

    if (afc_client_new(device, service, &afc) != AFC_E_SUCCESS || !afc) {
        LOG(ERROR) << "Could not create AFC client.";
        lockdownd_service_descriptor_free(service);
        lockdownd_client_free(client);
        idevice_free(device);
        return 1;
    }
    lockdownd_service_descriptor_free(service);

    // Recursively scan a directory (e.g., /DCIM)
    const char *rootPath = "/DCIM";
    LOG(INFO) << "Scanning directory: " << rootPath;
    scan_directory(afc, rootPath, skinThreshold || SKIN_THRESHOLD);

    // Cleanup
    afc_client_free(afc);
    lockdownd_client_free(client);
    idevice_free(device);

    return 0;
}
