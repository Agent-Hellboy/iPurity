#ifndef NSFW_DETECTOR_H
#define NSFW_DETECTOR_H

#include <string>
#include <memory>
#include <opencv2/opencv.hpp>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/model.h>
#include <tensorflow/lite/kernels/register.h>

class NSFWDetector {
public:
    NSFWDetector();
    ~NSFWDetector();
    
    bool initialize(const std::string& modelPath);
    float detectNSFW(const cv::Mat& image);
    
private:
    std::unique_ptr<tflite::FlatBufferModel> model;
    std::unique_ptr<tflite::Interpreter> interpreter;
    bool isInitialized;
    
    bool preprocessImage(const cv::Mat& input, float* output);
    static constexpr int INPUT_SIZE = 224;
    static constexpr float NSFW_THRESHOLD = 0.5f;
};

#endif // NSFW_DETECTOR_H 