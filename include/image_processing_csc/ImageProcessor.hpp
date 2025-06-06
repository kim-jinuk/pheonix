#pragma once
#include "running_control_csc/globals.hpp"


class CaptureUnit {

private :
    uint8_t frame_id=0;
    cv::VideoCapture cap;
    // GStreamer: YUYV raw → OpenCV로 수신
    std::string pipeline；
public :
    CaptureUnit();
    bool openCamera();
    void closeCamera();
    bool capture(cv::Mat& img, int& id_out);
    
};

class DevelopUnit{

public :
    void opt1(cv::Mat& img);
    void opt2(cv::Mat& img);
    void opt3(cv::Mat& img);
};

class OverlayUnit {

public :
    void overlay(cv::Mat& img);
};