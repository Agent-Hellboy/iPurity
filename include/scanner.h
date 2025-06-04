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
// Declare process_image_file to match implementation in src/scanner.cpp
void process_image_file(AfcClientPool* pool, const std::string& fullPath,
                        ScanStats& stats, float threshold);

// Directory scanning entry point.
void scan_directory(AfcClientPool* pool, const char* path, ScanStats& stats,
                    float threshold);

#endif  // SCANNER_H
