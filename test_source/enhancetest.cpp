#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>
#include <vector>
#include <numeric>

/**

*/

/**
1회
[CLAHE]    3018.52 ms
[Sharpen]  167.328 ms
[Denoise]  16410.7 ms
[Unsharp]  58.308 ms

*/
/**
10회 평균
[CLAHE]    433.364 ms
[Sharpen]  137.101 ms                                                           
[Denoise]  16375 ms                                                             
[Unsharp]  58.0174 ms 
 */
using namespace std;
using namespace std::chrono;

// 영상 프레임 구조체 정의
struct FrameData {
    cv::Mat img_bgr;
};

// 영상 개선 함수: 각 단계별 소요 시간 측정
void enhance(FrameData& frame,
             double& time_clahe,
             double& time_sharpen,
             double& time_denoise,
             double& time_unsharp) {
    cv::Mat& img = frame.img_bgr;

    auto t1 = high_resolution_clock::now();
    {
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
        cv::Mat lab; cv::cvtColor(img, lab, cv::COLOR_BGR2Lab);
        std::vector<cv::Mat> lab_planes(3);
        cv::split(lab, lab_planes);
        clahe->apply(lab_planes[0], lab_planes[0]);
        cv::merge(lab_planes, lab);
        cv::cvtColor(lab, img, cv::COLOR_Lab2BGR);
    }
    auto t2 = high_resolution_clock::now();
    time_clahe += duration_cast<microseconds>(t2 - t1).count() / 1000.0;

    t1 = high_resolution_clock::now();
    {
        cv::Mat sharp;
        cv::GaussianBlur(img, sharp, cv::Size(0, 0), 3);
        cv::addWeighted(img, 1.5, sharp, -0.5, 0, img);
    }
    t2 = high_resolution_clock::now();
    time_sharpen += duration_cast<microseconds>(t2 - t1).count() / 1000.0;

    t1 = high_resolution_clock::now();
    {
        cv::fastNlMeansDenoisingColored(img, img, 5, 5, 5, 11);
    }
    t2 = high_resolution_clock::now();
    time_denoise += duration_cast<microseconds>(t2 - t1).count() / 1000.0;

    t1 = high_resolution_clock::now();
    {
        cv::Mat blurred;
        cv::GaussianBlur(img, blurred, cv::Size(0, 0), 1.0);
        cv::addWeighted(img, 1.3, blurred, -0.3, 0, img);
    }
    t2 = high_resolution_clock::now();
    time_unsharp += duration_cast<microseconds>(t2 - t1).count() / 1000.0;
}

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "카메라 열기 실패\n";
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    FrameData frame;
    cap >> frame.img_bgr;
    if (frame.img_bgr.empty()) {
        std::cerr << "프레임 캡처 실패\n";
        return -1;
    }

    const int trials = 1;

    // 시간 누적 변수 초기화
    double time_clahe = 0.0;
    double time_sharpen = 0.0;
    double time_denoise = 0.0;
    double time_unsharp = 0.0;

    for (int i = 0; i < trials; ++i) {
        FrameData copy;
        frame.img_bgr.copyTo(copy.img_bgr);
        enhance(copy, time_clahe, time_sharpen, time_denoise, time_unsharp);
    }

    std::cout << "=== 100회 평균 영상 개선 시간 (ms) ===\n";
    std::cout << "[CLAHE]    " << time_clahe   / trials << " ms\n";
    std::cout << "[Sharpen]  " << time_sharpen / trials << " ms\n";
    std::cout << "[Denoise]  " << time_denoise / trials << " ms\n";
    std::cout << "[Unsharp]  " << time_unsharp / trials << " ms\n";

    return 0;
}
