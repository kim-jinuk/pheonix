
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


 cv::Mat ImageProcessor::enhance_edges(const cv::Mat &src, float strength = 1.0f) {
    v::Mat blur;
    cv::GaussianBlur(src, blur, {0,0}, 3);
    cv::Mat sharp;
    cv::addWeighted(src, 1.0 + strength, blur, -strength, 0, sharp);
    return sharp;

 }
cv::Mat ImageProcessor::enhance_contrast(const cv::Mat& src,
                                double clip_limit = 4.0,
                                cv::Size tile_grid = {32, 32})
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
    cv::Mat overlay(cv::Mat &src, vector<InferenceResult>& info) {

        for (const auto& res : info) {
            // 사각형 박스 그리기
            cv::Rect box(
                static_cast<int>(res.x1),
                static_cast<int>(res.y1),
                static_cast<int>(res.x2 - res.x1),
                static_cast<int>(res.y2 - res.y1)
            );
            cv::rectangle(src, box, cv::Scalar(0, 255, 0), 2); // 초록색 테두리

            // 텍스트 만들기
            std::string label = res.candidate + " " + cv::format("%.2f", res.score);

            int baseLine = 0;
            cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
            int top = std::max(static_cast<int>(res.y1), labelSize.height);

            // 텍스트 배경
            cv::rectangle(
                src,
                cv::Point(res.x1, top - labelSize.height),
                cv::Point(res.x1 + labelSize.width, top + baseLine),
                cv::Scalar(0, 255, 0), cv::FILLED
            );

            // 텍스트 그리기
            cv::putText(
                src, label,
                cv::Point(res.x1, top),
                cv::FONT_HERSHEY_SIMPLEX,
                0.5, cv::Scalar(0, 0, 0), 1
            );
        }
    }