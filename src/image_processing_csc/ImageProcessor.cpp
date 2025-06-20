
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


 void ImageProcessor::enhance_edges(cv::Mat& img, cv::Mat& buf) {
    if (img.empty() || !cam_opt.enhance_edges.load()) return;

    buf.create(img.size(), img.type());
    cv::blur(img, buf, {3,3});
    cv::addWeighted(img, 2.0f, buf, -1.0f, 0.0, img);

 }
void ImageProcessor::enhance_contrast(cv::Mat& img,  float gain , float gamma ,   int offset)
{
    if (img.empty() || cam_opt.enhance_contrast.load()) return;
    CV_Assert(img.depth() == CV_8U);   // 8-bit only

    // ───── LUT 한 번만 준비 ─────
    struct Lut {
        std::array<uchar,256> tbl;
        float g{}, gm1{};
        float gn{}, off{};
    };
    static Lut cache;

    if (cache.tbl[1] == 0 ||                // first-time
        cache.g  != gain   ||
        cache.gm1!= gamma  ||
        cache.off!= offset)
    {
        cache.g  = gain;
        cache.gm1= gamma;
        cache.off= offset;

        for (int i = 0; i < 256; ++i)
        {
            float x = i / 255.f;
            // gamma 보정
            x = std::pow(x, gamma);
            // 선형 gain + offset
            int v = static_cast<int>(x * 255.f * gain + offset + 0.5f);
            cache.tbl[i] = static_cast<uchar>(std::clamp(v, 0, 255));
        }
    }

    cv::LUT(img,
            cv::Mat(1, 256, CV_8UC1, cache.tbl.data()),
            img);          // in-place
}

void ImageProcessor::enhance_dehaze(cv::Mat& img) {

    if (img.empty() || cam_opt.enhance_dehaze.load()) return;
    CV_Assert(img.type() == CV_8UC3);

    const int rows = img.rows;
    const int cols = img.cols;

    for (int y = 0; y < rows; ++y) {
        uchar* ptr = img.ptr<uchar>(y);
        for (int x = 0; x < cols; ++x) {
            int b = ptr[3*x + 0];
            int g = ptr[3*x + 1];
            int r = ptr[3*x + 2];

            int min_rgb = std::min({r, g, b});
            float boost = 1.25f - (min_rgb / 255.f) * 0.25f;  // 밝은 영역은 덜 증가

            ptr[3*x + 0] = cv::saturate_cast<uchar>(b * boost);
            ptr[3*x + 1] = cv::saturate_cast<uchar>(g * boost);
            ptr[3*x + 2] = cv::saturate_cast<uchar>(r * boost);
        }
    }

}
cv::Mat ImageProcessor::ToPseudoIR(const cv::Mat& bgr) {

    if (!cam_opt.eo_ir.load()) return bgr;
    cv::Mat gray, ir3;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

   
    cv::bitwise_not(gray, gray);

    cv::cvtColor(gray, ir3, cv::COLOR_GRAY2BGR);   // 3채널로 복제
    return ir3;
}

cv::Mat overlay(cv::Mat &src, std::vector<InferenceResult>& info) {
    cv::Mat i;
    return i;
}