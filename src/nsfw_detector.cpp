#include "nsfw_detector.h"

#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>

/**
 * Check if a pixel (in YCrCb) is within a naive "skin" range.
 * This function assumes the pixel is in [Y, Cr, Cb] order.
 */
static bool isSkinPixel(const cv::Vec3b& ycrcb) {
    uchar Cr = ycrcb[1];
    uchar Cb = ycrcb[2];

    // Example naive thresholds for skin detection:
    //   140 < Cr < 175
    //   100 < Cb < 135
    if (Cr >= 140 && Cr <= 175 && Cb >= 100 && Cb <= 135) {
        return true;
    }
    return false;
}

bool naiveNSFWCheck(const std::string& imagePath, float skinThreshold) {
    // 1. Load the image in BGR format
    cv::Mat imgBGR = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (imgBGR.empty()) {
        std::cerr << "Could not load image: " << imagePath << std::endl;
        return false;
    }

    // 2. Convert to YCrCb
    cv::Mat imgYCrCb;
    cv::cvtColor(imgBGR, imgYCrCb, cv::COLOR_BGR2YCrCb);

    // 3. Count how many pixels fall in "skin" range
    long totalPixels = static_cast<long>(imgYCrCb.rows) * imgYCrCb.cols;
    long skinCount = 0;

    for (int y = 0; y < imgYCrCb.rows; y++) {
        const cv::Vec3b* rowPtr = imgYCrCb.ptr<cv::Vec3b>(y);
        for (int x = 0; x < imgYCrCb.cols; x++) {
            if (isSkinPixel(rowPtr[x])) {
                skinCount++;
            }
        }
    }

    // 4. Compute ratio of skin pixels
    float ratio =
        static_cast<float>(skinCount) / static_cast<float>(totalPixels);

    // 5. Return true if ratio >= threshold
    return (ratio >= skinThreshold);
}
