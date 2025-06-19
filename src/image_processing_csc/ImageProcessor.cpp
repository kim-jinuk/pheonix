
#include "image_processing_csc/ImageProcessor.hpp"
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
CaptureUnit::CaptureUnit() {
    pipeline= 
    "v4l2src device=/dev/video0 ! "
    "image/jpeg, width=640, height=480, framerate=30/1 ! "
    "jpegdec ! "
    "videoconvert ! "
    "appsink";
}

bool CaptureUnit::openCamera() {
    cap.open(0);
   // cap.open(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "[CaptureUnit] Failed to open camera" << std::endl;
        return false;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
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


 void ImageProcessor::enhance_edges(cv::Mat &img, float strength) {
    if (img.empty()) return;

    const float s = strength;
    static thread_local cv::Mat k;
    if (k.empty() || k.at<float>(1,1) != 1.f + 4*s) {
        k = (cv::Mat_<float>(3,3) <<
             0, -s, 0,
            -s, 1.f + 4*s, -s,
             0, -s, 0);
    }

    cv::filter2D(img, img, -1, k, {-1,-1}, 0, cv::BORDER_REPLICATE);

 }
void ImageProcessor::enhance_contrast(cv::Mat &img, float alpha , int beta)
{
    if (img.empty()) return;

    if (img.depth() != CV_8U)
        img.convertTo(img, CV_8U);

    img.convertTo(img, -1, alpha, beta); // in‑place 변환=
}
cv::Mat overlay(cv::Mat &src, std::vector<InferenceResult>& info) {
    cv::Mat i;
    return i;
}