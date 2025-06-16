
#include "image_processing_csc/ImageProcessor.hpp"
#include "running_control_csc/globals.hpp"


CaptureUnit::CaptureUnit() {
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
}

bool CaptureUnit::openCamera() {
    cap.open(0);
    if (!cap.isOpened()) {
        std::cerr << "[CaptureUnit] Failed to open camera" << std::endl;
        return false;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

     // --- 카메라 설정 확인 ---
    double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = cap.get(cv::CAP_PROP_FPS);
    double fourcc = cap.get(cv::CAP_PROP_FOURCC);

    std::cout << "=== 카메라 설정 확인 ===\n";
    std::cout << "해상도 : " << width << " x " << height << "\n";
    std::cout << "FPS    : " << fps << "\n";
    std::cout << "FOURCC : "
              << static_cast<char>(static_cast<int>(fourcc) & 0xFF)
              << static_cast<char>((static_cast<int>(fourcc) >> 8) & 0xFF)
              << static_cast<char>((static_cast<int>(fourcc) >> 16) & 0xFF)
              << static_cast<char>((static_cast<int>(fourcc) >> 24) & 0xFF)
              << "\n\n";
    return true;
}

void CaptureUnit::closeCamera() {
    if (cap.isOpened()) {
        cap.release();
        std::cout << "[CaptureUnit] Camera released." << std::endl;
    }
}

 bool CaptureUnit::capture(std::shared_ptr<FrameData>& frame) {

    if (!cap.isOpened()) return false;

    cap >> frame->img_bgr;
    frame->frame_id = frame_id++;

    return !frame->img_bgr.empty();
}

void DevelopUnit::enhance(std::shared_ptr<FrameData>& frame) {
    cv::Mat& img = frame->img_bgr;

    if (cam_opt.use_clahe.load()) {
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
        cv::Mat lab; cv::cvtColor(img, lab, cv::COLOR_BGR2Lab);
        std::vector<cv::Mat> lab_planes(3);
        cv::split(lab, lab_planes);
        clahe->apply(lab_planes[0], lab_planes[0]);
        cv::merge(lab_planes, lab);
        cv::cvtColor(lab, img, cv::COLOR_Lab2BGR);
    }

    if (cam_opt.use_sharpen.load()) {
        cv::Mat sharp;
        cv::GaussianBlur(img, sharp, cv::Size(0, 0), 3);
        cv::addWeighted(img, 1.5, sharp, -0.5, 0, img);
    }

    if (cam_opt.use_denoise.load()) {
        cv::fastNlMeansDenoisingColored(img, img, 10, 10, 7, 21);
    }

    if (cam_opt.use_unsharp.load()) {
        cv::Mat blurred;
        cv::GaussianBlur(img, blurred, cv::Size(0, 0), 1.0);
        cv::addWeighted(img, 1.3, blurred, -0.3, 0, img);
    }
}


void OverlayUnit::overlay(std::shared_ptr<FrameData>& frame) {

    std::cout << "overlay" << std::endl;
}