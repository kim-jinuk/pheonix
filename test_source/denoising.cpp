#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>

/**
Run 0: 18976 ms                                                                 
Run 1: 15241 ms                                                                 
Run 2: 15125 ms                                                                 
Run 3: 15106 ms                                                                 
Run 4: 15305 ms                                                                 
Run 5: 15320 ms                                                                 
Run 6: 15058 ms                                                                 
Run 7: 15320 ms  
*/
/*
neon 빌드 후,
Run 0: 18484 ms
Run 1: 15414 ms
Run 2: 15577 ms
Run 3: 15544 ms
Run 4: 15712 ms

*/
using namespace std;
using namespace std::chrono;

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "카메라 열기 실패\n";
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "프레임 캡처 실패\n";
        return -1;
    }

    const int trials = 10;

    std::cout << "=== fastNlMeansDenoisingColored() 반복 측정 결과 ===\n";

    for (int i = 0; i < trials; ++i) {
        cv::Mat img_copy = frame.clone();

        auto t1 = high_resolution_clock::now();
        cv::fastNlMeansDenoisingColored(img_copy, img_copy, 10, 10, 7, 21);
        auto t2 = high_resolution_clock::now();

        double elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "Run " << i << ": " << elapsed << " ms\n";
    }

    return 0;
}

// int main() {
//     std::cout << cv::getBuildInformation() << std::endl;
//     return 0;
// }