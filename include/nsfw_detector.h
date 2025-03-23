#ifndef NSFW_DETECTOR_H
#define NSFW_DETECTOR_H

#include <string>

const float DEFAULT_SKIN_THRESHOLD = 0.6f;

/**
 * Loads an image from 'imagePath', converts to YCrCb,
 * and checks the ratio of "skin" pixels. If ratio >= skinThreshold,
 * returns true (flag as NSFW). Otherwise false.
 */
bool naiveNSFWCheck(const std::string& imagePath, float skinThreshold  = DEFAULT_SKIN_THRESHOLD);

#endif // NSFW_DETECTOR_H
