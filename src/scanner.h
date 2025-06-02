#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include "afc_client_pool.h"
#include "nsfw_detector.h"

struct ScanStats {
    int totalFiles = 0;
    int nsfwFiles = 0;
    int safeFiles = 0;
    std::vector<std::string> nsfwFilesList;
};

void scan_directory(AfcClientPool* pool, const char* path, ScanStats& stats,
                   float threshold, NSFWDetector* detector);

#endif // SCANNER_H 