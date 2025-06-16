#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>
#include <vector>
#include <numeric>

using namespace std;
using namespace std::chrono;

// 영상 프레임 구조체 정의
struct FrameData {
    cv::Mat img_bgr;
};

// LUT 기반 감마 보정 테이블 생성
cv::Mat createGammaLUT(float gamma) {
    cv::Mat lut(1, 256, CV_8UC1);
    for (int i = 0; i < 256; ++i) {
        lut.at<uchar>(i) = cv::saturate_cast<uchar>(pow(i / 255.0, gamma) * 255.0);
    }
    return lut;
}

// 영상 개선 함수: Python 코드와 동일한 동작
void enhance(FrameData& frame,
             double& time_clahe,
             double& time_gamma,
             double& time_denoise,
             double& time_unsharp) {
    cv::Mat& img = frame.img_bgr;

    // CLAHE (tile=4, clipLimit=1.5)
    auto t1 = high_resolution_clock::now();
    {
        // LightCLAHE 설정: 낮은 clipLimit과 큰 tileGridSize
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(1.0, cv::Size(16, 16));
        cv::Mat lab;
        cv::cvtColor(img, lab, cv::COLOR_BGR2Lab);
        std::vector<cv::Mat> lab_planes(3);
        cv::split(lab, lab_planes);
        clahe->apply(lab_planes[0], lab_planes[0]);
        cv::merge(lab_planes, lab);
        cv::cvtColor(lab, img, cv::COLOR_Lab2BGR);
    }

    auto t2 = high_resolution_clock::now();
    time_clahe += duration_cast<microseconds>(t2 - t1).count() / 1000.0;

    // Gamma LUT 보정 (gamma=1.5)
    t1 = high_resolution_clock::now();
    {
        static cv::Mat gammaLUT = createGammaLUT(1.5f);
        cv::LUT(img, gammaLUT, img);
    }
    t2 = high_resolution_clock::now();
    time_gamma += duration_cast<microseconds>(t2 - t1).count() / 1000.0;

    // Gaussian Denoise (ksize=3, sigma=0.0)
    t1 = high_resolution_clock::now();
    {
        cv::GaussianBlur(img, img, cv::Size(3, 3), 0.0);
    }
    t2 = high_resolution_clock::now();
    time_denoise += duration_cast<microseconds>(t2 - t1).count() / 1000.0;

    // Unsharp Mask (strength=1.0)
    t1 = high_resolution_clock::now();
    {
        cv::Mat blurred;
        cv::GaussianBlur(img, blurred, cv::Size(0, 0), 3);
        cv::addWeighted(img, 2.0, blurred, -1.0, 0, img);
    }
    t2 = high_resolution_clock::now();
    time_unsharp += duration_cast<microseconds>(t2 - t1).count() / 1000.0;
}

int main() {
    //std::cout << cv::getBuildInformation() << std::endl;

  /* cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "카메라 열기 실패\n";
        return -1;
    }*/
    cv::VideoCapture cap("/dev/video0", cv::CAP_V4L2);
    if (!cap.isOpened()) {
        std::cerr << "카메라 열기 실패\n";
        return -1;
    }
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G')); // 압축
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);

    // 실제 적용된 설정 확인
    double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = cap.get(cv::CAP_PROP_FPS);
    int fourcc = static_cast<int>(cap.get(cv::CAP_PROP_FOURCC));
    char fourcc_str[] = {
        static_cast<char>(fourcc & 0xFF),
        static_cast<char>((fourcc >> 8) & 0xFF),
        static_cast<char>((fourcc >> 16) & 0xFF),
        static_cast<char>((fourcc >> 24) & 0xFF),
        0
    };
  //  cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
   // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    std::cout << "=== 카메라 설정 확인 ===\n";
    std::cout << "해상도 : " << width << " x " << height << "\n";
    std::cout << "FPS    : " << fps << "\n";
    std::cout << "FOURCC : " << fourcc_str << "\n";
    FrameData frame;
    cap >> frame.img_bgr;
    if (frame.img_bgr.empty()) {
        std::cerr << "프레임 캡처 실패\n";
        return -1;
    }

    const int trials = 100;

    // 시간 누적 변수 초기화
    double time_clahe = 0.0;
    double time_gamma = 0.0;
    double time_denoise = 0.0;
    double time_unsharp = 0.0;

    for (int i = 0; i < trials; ++i) {
        FrameData copy;
        frame.img_bgr.copyTo(copy.img_bgr);
        enhance(copy, time_clahe, time_gamma, time_denoise, time_unsharp);
    }

    std::cout << "=== " << trials << "회 평균 영상 개선 시간 (ms) ===\n";
    std::cout << "[CLAHE]    " << time_clahe   / trials << " ms\n";
    std::cout << "[Gamma]    " << time_gamma   / trials << " ms\n";
    std::cout << "[Denoise]  " << time_denoise / trials << " ms\n";
    std::cout << "[Unsharp]  " << time_unsharp / trials << " ms\n";

    return 0;
}
