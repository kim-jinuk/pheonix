#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>
// 평균 3.75832 ms 
int main() {
    // 640x480 BGR 이미지 생성 (임의 데이터)
    cv::Mat original(480, 640, CV_8UC3, cv::Scalar(100, 150, 200));

    const int trials = 100;
    double total_ms = 0;

    for (int i = 0; i < trials; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        // clone 수행
        cv::Mat cloned = original.clone();

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        total_ms += ms;
        std::cout << "[" << i << "] clone time: " << ms << " ms" << std::endl;
    }

    std::cout << "\nAverage clone time over " << trials << " trials: "
              << (total_ms / trials) << " ms" << std::endl;

    return 0;
}
