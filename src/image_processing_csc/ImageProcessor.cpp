
#include "image_processing_csc/ImageProcessor.hpp"
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
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


 cv::Mat ImageProcessor::enhance_edges(const cv::Mat &src, float strength) {
    cv::Mat blur;
    cv::GaussianBlur(src, blur, {0,0}, 3);
    cv::Mat sharp;
    cv::addWeighted(src, 1.0 + strength, blur, -strength, 0, sharp);
    return sharp;

 }
cv::Mat ImageProcessor::enhance_contrast(const cv::Mat& src,
                                double clip_limit,
                                cv::Size tile_grid)
{
    cv::Mat out;

    if (src.channels() == 3) {
        // ----- 컬러 프레임 -----
        cv::Mat lab;
        cv::cvtColor(src, lab,
                     src.type() == CV_8UC3 ? cv::COLOR_BGR2Lab
                                           : cv::COLOR_RGB2Lab);

        std::vector<cv::Mat> channels;
        cv::split(lab, channels);          // L, a, b

        // CLAHE(Contrast Limited Adaptive Histogram Equalization)
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clip_limit, tile_grid);
        clahe->apply(channels[0], channels[0]);

        cv::merge(channels, lab);
        cv::cvtColor(lab, out,
                     src.type() == CV_8UC3 ? cv::COLOR_Lab2BGR
                                           : cv::COLOR_Lab2RGB);
    }
    else {
        // ----- 그레이스케일 -----
        cv::equalizeHist(src, out);
    }
    return out;


}
cv::Mat overlay(cv::Mat &src, std::vector<InferenceResult>& info) {
    cv::Mat i;
    return i;
}