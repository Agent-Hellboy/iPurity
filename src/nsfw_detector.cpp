#include "nsfw_detector.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

NSFWDetector::NSFWDetector() : isInitialized(false) {}

NSFWDetector::~NSFWDetector() {
    // Cleanup resources
    interpreter.reset();
    model.reset();
}

std::string find_model_path() {
    // Try Homebrew's share directory first
    std::string homebrew_share = "/usr/local/share/ipurity/nsfw_model.tflite";
    if (fs::exists(homebrew_share)) {
        return homebrew_share;
    }

    // Try Apple Silicon Homebrew location
    homebrew_share = "/opt/homebrew/share/ipurity/nsfw_model.tflite";
    if (fs::exists(homebrew_share)) {
        return homebrew_share;
    }

    // Try user's home directory
    const char* home = getenv("HOME");
    if (home) {
        std::string user_model = std::string(home) + "/ipurity/models/nsfw_model.tflite";
        if (fs::exists(user_model)) {
            return user_model;
        }
    }

    // Fall back to local models directory for development
    return "models/nsfw_model.tflite";
}

bool NSFWDetector::initialize(const std::string& model_path) {
    std::string actual_path = model_path.empty() ? find_model_path() : model_path;
    
    if (!fs::exists(actual_path)) {
        std::cerr << "Error: Model file not found at " << actual_path << std::endl;
        std::cerr << "Please ensure the model is installed in one of these locations:" << std::endl;
        std::cerr << "  - /usr/local/share/ipurity/nsfw_model.tflite" << std::endl;
        std::cerr << "  - /opt/homebrew/share/ipurity/nsfw_model.tflite" << std::endl;
        std::cerr << "  - ~/ipurity/models/nsfw_model.tflite" << std::endl;
        std::cerr << "  - ./models/nsfw_model.tflite" << std::endl;
        return false;
    }

    // Load the TensorFlow Lite model
    model = tflite::FlatBufferModel::BuildFromFile(actual_path.c_str());
    if (!model) {
        std::cerr << "Failed to load model: " << actual_path << std::endl;
        return false;
    }

    // Build the interpreter
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder builder(*model, resolver);
    builder(&interpreter);
    
    if (!interpreter) {
        std::cerr << "Failed to build interpreter" << std::endl;
        return false;
    }

    // Allocate tensors
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        std::cerr << "Failed to allocate tensors" << std::endl;
        return false;
    }

    isInitialized = true;
    return true;
}

bool NSFWDetector::preprocessImage(const cv::Mat& input, float* output) {
    cv::Mat resized;
    cv::resize(input, resized, cv::Size(INPUT_SIZE, INPUT_SIZE));
    
    // Convert to RGB and normalize
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32FC3, 1.0/255.0);
    
    // Copy data to input tensor
    float* input_tensor = interpreter->typed_input_tensor<float>(0);
    for (int y = 0; y < INPUT_SIZE; y++) {
        for (int x = 0; x < INPUT_SIZE; x++) {
            cv::Vec3f pixel = rgb.at<cv::Vec3f>(y, x);
            *input_tensor++ = pixel[0];
            *input_tensor++ = pixel[1];
            *input_tensor++ = pixel[2];
        }
    }
    
    return true;
}

float NSFWDetector::detectNSFW(const cv::Mat& image) {
    if (!isInitialized) {
        std::cerr << "Detector not initialized" << std::endl;
        return -1.0f;
    }

    if (image.empty()) {
        std::cerr << "Input image is empty" << std::endl;
        return -1.0f;
    }

    // Preprocess image
    float* input = interpreter->typed_input_tensor<float>(0);
    if (!preprocessImage(image, input)) {
        return -1.0f;
    }

    // Run inference
    if (interpreter->Invoke() != kTfLiteOk) {
        std::cerr << "Failed to run inference" << std::endl;
        return -1.0f;
    }

    // Get output
    float* output = interpreter->typed_output_tensor<float>(0);
    return output[1]; // Assuming output[1] is the NSFW probability
}

