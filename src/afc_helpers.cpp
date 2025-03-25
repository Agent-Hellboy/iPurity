// afc_helpers.cpp
#include "afc_helpers.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

char* build_full_path(const char* directory, const char* entry) {
    char* fullPath = nullptr;
    asprintf(&fullPath, "%s/%s", directory, entry);
    return fullPath;
}

bool is_directory(afc_client_t afc, const char* fullPath) {
    char** fileInfo = NULL;
    if (afc_get_file_info(afc, fullPath, &fileInfo) != AFC_E_SUCCESS ||
        !fileInfo) {
        return false;
    }
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
    return isDir;
}

// Check if the file extension indicates an image file.
bool is_image_file(const std::string& filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) return false;

    std::string ext = filePath.substr(dotPos);
    for (auto& c : ext) c = tolower(c);

    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" ||
            ext == ".bmp" || ext == ".tiff" || ext == ".webp");
}