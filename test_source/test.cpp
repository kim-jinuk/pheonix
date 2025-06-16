#include <iostream>
#include <opencv2/opencv.hpp>


int main() {

    // cv::VideoCapture cap(0); // /dev/video0
    // if (!cap.isOpened()) {
    //     std::cerr << "카메라 열기 실패\n";
    //     return -1;
    // }

    // // 설정 요청
    // cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    // cap.set(cv::CAP_PROP_FPS, 30);
    // cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));

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

    std::cout << "=== 카메라 설정 확인 ===\n";
    std::cout << "해상도 : " << width << " x " << height << "\n";
    std::cout << "FPS    : " << fps << "\n";
    std::cout << "FOURCC : " << fourcc_str << "\n";

    return 0;
}
