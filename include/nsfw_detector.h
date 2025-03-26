#ifndef NAIVE_NSFW_SCANNER_H
#define NAIVE_NSFW_SCANNER_H

#include "INsfwScanner.h"
#include <opencv2/opencv.hpp>
#include <string>

class NaiveNsfwScanner : public INsfwScanner {
public:
    bool scan(const std::string& filePath, float threshold) override;

private:
    bool isSkinPixel(const cv::Vec3b& ycrcb) const;
    bool naiveNSFWCheck(const std::string& imagePath, float skinThreshold) const;
};

#endif // NAIVE_NSFW_SCANNER_H


