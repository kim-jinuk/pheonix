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
    void enhance_edges(cv::Mat& img, cv::Mat& buf);
    void enhance_contrast(cv::Mat& img,
                  float gain   = 1.1f,   // 대비 10 % 증가
                  float gamma  = 0.9f,   // 살짝 밝게
                  int   offset = 0) ;     // 밝기 오프셋
    void enhance_dehaze(cv::Mat& img);
    cv::Mat ToPseudoIR(const cv::Mat& bgr);
    cv::Mat overlay(cv::Mat &src, std::vector<InferenceResult>& info);
};


