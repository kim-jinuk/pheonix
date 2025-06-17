#pragma once
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>

class CaptureUnit {

private :
    uint32_t frame_id=0;
    cv::VideoCapture cap;
public :
    CaptureUnit();
    bool openCamera();
    void closeCamera();
    bool capture(std::shared_ptr<FrameData>& frame);
    
};

class ImageProcessor {

public :
    cv::Mat enhance_edges(const cv::Mat &src, float strength = 1.0f);
    cv::Mat enhance_contrast(const cv::Mat& src,
                                double clip_limit = 4.0,
                                cv::Size tile_grid = {32, 32});
    cv::Mat overlay(cv::Mat &src, vector<InferenceResult>& info);
};



