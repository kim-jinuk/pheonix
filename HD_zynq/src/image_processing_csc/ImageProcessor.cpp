
#include "image_procssing_csc/ImageProcessor.hpp"


CaptureUnit::CaptureUnit(std::string pipe) {
    pipeline=pipe;
}

bool CaptureUnit::openCamera() {
    return cap.open(pipeline, cv::CAP_GSTREAMER);
}

void CaptureUnit::closeCamera() {
    if (cap.isOpened()) {
        cap.release();
        std::cout << "[CaptureUnit] Camera released." << std::endl;
    }
}

 bool CaptureUnit::capture(cv::Mat& img, int& id_out); {

     if (!cap.isOpened()) return false;
        cap >> img;
        id_out = frame_id++;
        return !img.empty();
}

void DevelopUnit::opt1(cv::Mat& img) {
    std::cout << "opt1" << std::endl;
}

void DevelopUnit::opt2(cv::Mat& img) {
    std::cout << "opt2" << std::endl;
}

void DevelopUnit::opt3(cv::Mat& img) {
    std::cout << "opt3" << std::endl;
}

void OverlayUnit::overlay(cv::Mat& img) {

    std::cout << "overlay" << std::endl;
}