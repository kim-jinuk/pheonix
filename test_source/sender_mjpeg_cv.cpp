#include <opencv2/opencv.hpp>
#include <chrono>
#include <vector>
#include <iostream>

#define MAX_FRAMES 100
// 17초
int main() {
    // GStreamer MJPEG 수신 파이프라인
    std::string pipeline = 
        "v4l2src device=/dev/video0 ! "
        "image/jpeg, width=640, height=480, framerate=30/1 ! "
        "appsink";

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "[ERR] Failed to open camera with GStreamer MJPEG pipeline\n";
        return -1;
    }

    for (int frame_id = 0; frame_id < MAX_FRAMES; ++frame_id) {
        auto t0 = std::chrono::high_resolution_clock::now();

        cv::Mat jpeg_mat;
        cap >> jpeg_mat;
        if (jpeg_mat.empty()) {
            std::cerr << "[WARN] Empty frame\n";
            continue;
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        cv::Mat bgr_frame = cv::imdecode(jpeg_mat, cv::IMREAD_COLOR);
        auto t2 = std::chrono::high_resolution_clock::now();

        double read_time   = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double decode_time = std::chrono::duration<double, std::milli>(t2 - t1).count();
        double total_time  = std::chrono::duration<double, std::milli>(t2 - t0).count();

        std::cout << "[Frame " << frame_id << "] "
                  << "Read: " << read_time << " ms, "
                  << "Decode: " << decode_time << " ms, "
                  << "Total: " << total_time << " ms\n";
    }

    return 0;
}
