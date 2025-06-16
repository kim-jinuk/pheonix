#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>
#include <vector>
#include <numeric>
/**
�박�스 1�개 �평�균 �오�버�레�이 �시�간: 0.1968 ms
�박�스 2�개 �평�균 �오�버�레�이 �시�간: 0.2957 ms
�박�스 3�개 �평�균 �오�버�레�이 �시�간: 0.4166 ms
�박�스 4�개 �평�균 �오�버�레�이 �시�간: 0.5434 ms
�박�스 5�개 �평�균 �오�버�레�이 �시�간: 0.5478 ms

*/
int main() {
    cv::VideoCapture cap(0); // 캠 열기
    if (!cap.isOpened()) {
        std::cerr << "카메라 열기 실패\n";
        return -1;
    }

    const int trials = 10;

    for (int box_count = 1; box_count <= 5; ++box_count) {
        std::vector<double> durations;

        for (int i = 0; i < trials; ++i) {
            cv::Mat frame;
            cap >> frame;  // ⚠️ 캡처는 측정 대상 아님

            // 측정 시작
            auto start = std::chrono::high_resolution_clock::now();

            for (int b = 0; b < box_count; ++b) {
                int x = 50 + b * 30;
                int y = 50 + b * 20;
                cv::rectangle(frame, cv::Rect(x, y, 100, 80), cv::Scalar(0, 255, 0), 2);
            }

            // 측정 종료
            auto end = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
            durations.push_back(ms);
        }

        double sum = std::accumulate(durations.begin(), durations.end(), 0.0);
        double avg = sum / durations.size();

        std::cout << "박스 " << box_count << "개 평균 오버레이 시간: " << avg << " ms" << std::endl;
    }

    return 0;
}
