#pragma once
#include <libimobiledevice/afc.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include <string>
/**
 * Build the full path for a directory entry.
 */
char* build_full_path(const char* directory, const char* entry);

/**
 * Check if the given fullPath represents a directory.
 */
bool is_directory(afc_client_t afc, const char* fullPath);

/**
 * Check if the file extension indicates an image file.
 */
bool is_image_file(const std::string& filePath);
