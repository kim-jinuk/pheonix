#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>
#include <vector>
#include <numeric>

using namespace std;
using namespace std::chrono;

double measure_overlay_time(int box_count, int trials) {
    vector<double> durations;

    for (int t = 0; t < trials; ++t) {
        // 640x480 회색 배경 이미지 생성
        cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(128, 128, 128));

        // 임의의 사각형 좌표들 생성
        vector<cv::Rect> boxes;
        for (int i = 0; i < box_count; ++i) {
            int x = 50 + i * 30;
            int y = 50 + i * 20;
            boxes.emplace_back(x, y, 100, 80); // width, height 고정
        }

        // 측정 시작
        auto start = high_resolution_clock::now();

        // 박스 그리기
        for (const auto& box : boxes) {
            cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
        }

        auto end = high_resolution_clock::now();
        double ms = duration_cast<microseconds>(end - start).count() / 1000.0;
        durations.push_back(ms);
    }

    // 평균 계산
    double sum = accumulate(durations.begin(), durations.end(), 0.0);
    return sum / durations.size();
}

int main() {
    const int trials = 10;

    for (int box_count = 1; box_count <= 5; ++box_count) {
        double avg_time = measure_overlay_time(box_count, trials);
        cout << "박스 " << box_count << "개 평균 시간: " << avg_time << " ms" << endl;
    }

    return 0;
}
