#include "nsfw_detector.h"

bool NaiveNsfwScanner::scan(const std::string& filePath, float threshold) {
    return naiveNSFWCheck(filePath, threshold);
}

bool NaiveNsfwScanner::isSkinPixel(const cv::Vec3b& ycrcb) const {
    uchar Cr = ycrcb[1];
    uchar Cb = ycrcb[2];
    return (Cr >= 140 && Cr <= 175 && Cb >= 100 && Cb <= 135);
}

bool NaiveNsfwScanner::naiveNSFWCheck(const std::string& imagePath, float skinThreshold) const {
    cv::Mat imgBGR = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (imgBGR.empty()) {
        std::cerr << "Could not load image: " << imagePath << std::endl;
        return false;
    }

    cv::Mat imgYCrCb;
    cv::cvtColor(imgBGR, imgYCrCb, cv::COLOR_BGR2YCrCb);

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

    float ratio = static_cast<float>(skinCount) / static_cast<float>(totalPixels);
    return (ratio >= skinThreshold);
}
