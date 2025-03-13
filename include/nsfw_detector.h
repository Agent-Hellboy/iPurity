#ifndef NSFW_DETECTOR_H
#define NSFW_DETECTOR_H

#include <string>

/**
 * Loads an image from 'imagePath', converts to YCrCb,
 * and checks the ratio of "skin" pixels. If ratio >= skinThreshold,
 * returns true (flag as NSFW). Otherwise false.
 */
bool naiveNSFWCheck(const std::string& imagePath, float skinThreshold);

#endif // NSFW_DETECTOR_H
