#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// libimobiledevice
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/afc.h>

// Our naive NSFW detection
#include "nsfw_detector.h"

/**
 * Example function to download a file from iPhone (via AFC)
 * and save it locally. (Stub - not fully robust.)
 *
 * Returns true on success, false on error.
 */
static bool download_file(afc_client_t afc, const char* remotePath, const char* localPath)
{
    // Open remote file for reading
    uint64_t fileRef = 0;
    if (afc_file_open(afc, remotePath, AFC_FOPEN_RDONLY, &fileRef) != AFC_E_SUCCESS) {
        std::cerr << "Failed to open remote file: " << remotePath << std::endl;
        return false;
    }

    // Open local file for writing
    FILE* outFile = std::fopen(localPath, "wb");
    if (!outFile) {
        std::cerr << "Failed to open local file: " << localPath << std::endl;
        afc_file_close(afc, fileRef);
        return false;
    }

    const size_t BUF_SIZE = 4096;
    char buffer[BUF_SIZE];
    uint32_t bytesRead = 0;

    // Read loop
    while (true) {
        afc_error_t readErr = afc_file_read(afc, fileRef, buffer, BUF_SIZE, &bytesRead);
        if (readErr != AFC_E_SUCCESS || bytesRead == 0) {
            // Either error or EOF
            break;
        }
        std::fwrite(buffer, 1, bytesRead, outFile);
    }

    afc_file_close(afc, fileRef);
    std::fclose(outFile);

    return true;
}

static void scan_directory(afc_client_t afc, const char *path);

/**
 * Main entry point:
 * 1. Connect to the first detected iOS device.
 * 2. Create a lockdownd client.
 * 3. Start the AFC service.
 * 4. Recursively scan a directory (e.g., /DCIM) to find files.
 * 5. Download each image and run naiveNSFWCheck on it.
 */
int main(int argc, char *argv[])
{
    idevice_t device = NULL;
    lockdownd_client_t client = NULL;
    lockdownd_service_descriptor_t service = NULL;
    afc_client_t afc = NULL;

    // Connect to device
    if (idevice_new(&device, NULL) != IDEVICE_E_SUCCESS) {
        std::cerr << "No iOS device found. Is it plugged in and trusted?" << std::endl;
        return 1;
    }

    // Create a lockdownd client
    if (lockdownd_client_new_with_handshake(device, &client, "afc_scanner") != LOCKDOWN_E_SUCCESS) {
        std::cerr << "Could not create lockdownd client." << std::endl;
        idevice_free(device);
        return 1;
    }

    // Start AFC service
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

    // Cleanup the service descriptor
    lockdownd_service_descriptor_free(service);

    // For demo, we set a default "skin threshold"
    // You might want to parse from argv
    float skinThreshold = 0.3f;

    // Recursively scan a folder (e.g., /DCIM for photos/videos)
    const char *rootPath = "/DCIM";
    std::cout << "Scanning directory: " << rootPath << std::endl;
    scan_directory(afc, rootPath);

    // Cleanup
    afc_client_free(afc);
    lockdownd_client_free(client);
    idevice_free(device);

    return 0;
}

/**
 * Recursively list files and directories on the iPhone via AFC.
 * In your real code, you'd:
 *   1) Check if it's an image/video (by extension).
 *   2) Download the file to a local path.
 *   3) Run naiveNSFWCheck(...) on the downloaded file.
 */
static void scan_directory(afc_client_t afc, const char *path)
{
    char **dirList = NULL;
    afc_error_t err = afc_read_directory(afc, path, &dirList);
    if (err != AFC_E_SUCCESS) {
        std::cerr << "Error reading directory " << path
                  << " (afc error " << err << ")" << std::endl;
        return;
    }

    for (int i = 0; dirList[i]; i++) {
        const char *entry = dirList[i];

        // Skip "." and ".."
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) {
            continue;
        }

        // Build full path
        char *fullPath = nullptr;
        asprintf(&fullPath, "%s/%s", path, entry);

        // Retrieve file info
        char **fileInfo = NULL;
        if (afc_get_file_info(afc, fullPath, &fileInfo) == AFC_E_SUCCESS && fileInfo) {
            // We'll check st_ifmt in the returned dictionary
            // st_ifmt can be "S_IFDIR" or "S_IFREG"
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
                // Recursively scan subdirectory
                scan_directory(afc, fullPath);
            } else {
                // It's a file. Let's see if it's a .jpg or .png
                std::string filePathStr(fullPath);
                // Naive extension check
                if (filePathStr.size() > 4) {
                    std::string ext = filePathStr.substr(filePathStr.size() - 4);
                    for (auto &c : ext) c = tolower(c);

                    if (ext == ".jpg" || ext == ".png") {
                        std::cout << "Found image file: " << fullPath << std::endl;

                        // 1) Download to local
                        std::string localFile = "/tmp/ios_" + filePathStr.substr(filePathStr.find_last_of("/")+1);
                        if (download_file(afc, fullPath, localFile.c_str())) {
                            // 2) Run naive NSFW check
                            bool isNSFW = naiveNSFWCheck(localFile);
                            if (isNSFW) {
                                std::cout << "[NSFW DETECTED] " << localFile << std::endl;
                            } else {
                                std::cout << "[SAFE] " << localFile << std::endl;
                            }
                            // 3) Optionally remove the local file if you don't need it
                            // remove(localFile.c_str());
                        }
                    }
                }
            }
        }

        free(fullPath);
    }

    afc_dictionary_free(dirList);
}
