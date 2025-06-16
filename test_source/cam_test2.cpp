#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

int main() {
    const std::string symlink_path = "/dev/video-cam";
    bool last_exists = false;
    bool camera_opened = false;

    std::cout << "[INFO] Monitoring camera at " << symlink_path << " ...\n";
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    while (true) {
        bool exists = fs::exists(symlink_path);

        if (exists) 
        {
            std::cout << " exist" << std::endl;
            if (cap.isOpened()){
                cv::Mat frame;
                if (cap.read(frame)) {
                    std::cout << "[INFO] Captured frame.\n";
                } else {
                    std::cerr << "[WARN] Failed to capture frame.\n";
                }
            }
            else {
                std::cout << "reopen.\n";
                cap.open(0, cv::CAP_V4L2);
            }
        }
        else {
            std::cout << " no" << std::endl;
            if (cap.isOpened()){
                std::cout << "release.\n";
                cap.release();
               
            }
        }
       
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}
