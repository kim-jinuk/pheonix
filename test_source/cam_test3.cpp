#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

int main() {
    const std::string device_path = "/dev/video0";
    bool camera_opened = false;

    std::cout << "[INFO] Monitoring camera at " << device_path << " ...\n";
    cv::VideoCapture cap;

    while (true) {
        bool exists = fs::exists(device_path);

        if (exists) {
            std::cout << "[INFO] Device exists\n";

            if (!cap.isOpened()) {
                std::cout << "[INFO] Attempting to open camera...\n";
                if (!cap.open(0, cv::CAP_V4L2)) {
                    std::cerr << "[ERROR] Failed to open camera\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    continue;
                }
            }

            cv::Mat frame;
            if (cap.read(frame)) {
                std::cout << "[INFO] Captured frame.\n";
            } else {
                std::cerr << "[WARN] Failed to capture frame.\n";
            }
        } 
        else {
            std::cout << "[INFO] Device not present.\n";
            if (cap.isOpened()) {
                std::cout << "[INFO] Releasing camera...\n";
                cap.release();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}
