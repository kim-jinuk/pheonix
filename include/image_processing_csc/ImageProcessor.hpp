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

class DevelopUnit{

public :
    static void enhance(std::shared_ptr<FrameData>& frame);
};

class OverlayUnit {

public :
    void overlay(std::shared_ptr<FrameData>& frame);
};