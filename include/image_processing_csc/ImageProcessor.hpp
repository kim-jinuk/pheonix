#pragma once
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
class CaptureUnit {

private :
    uint32_t frame_id=0;
    cv::VideoCapture cap;
    std::string pipeline;
public :
    CaptureUnit();
    bool openCamera();
    void closeCamera();
    bool capture(std::shared_ptr<FrameData>& frame);
    
};

class ImageProcessor {

public :
    void enhance_edges(cv::Mat &img, float strength = 1.0f);
    void enhance_contrast(cv::Mat &img, float alpha = 1.3f, int beta = -20);
    cv::Mat overlay(cv::Mat &src, std::vector<InferenceResult>& info);
};


