#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include <vector>

#include "afc_client_pool.h"

// Structure to hold scan statistics.
struct ScanStats {
    int totalFiles = 0;
    int nsfwFiles = 0;
    int safeFiles = 0;
    std::vector<std::string> nsfwFilesList;
};

// Scanner functions.
bool download_file(afc_client_t afc, const char* remotePath,
                   const char* localPath);
void process_image_file(AfcClientPool* pool, const char* fullPath,
                        ScanStats& stats, float threshold);
void scan_directory(AfcClientPool* pool, const char* path, ScanStats& stats,
                    float threshold);

#endif  // SCANNER_H
