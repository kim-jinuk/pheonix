#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <thread>
#include <chrono>

constexpr const char* SYMLINK_PATH = "/dev/video-cam";
constexpr const char* TARGET_CAMERA_NAME = "HD Pro Webcam C920";

// 심볼릭 링크에서 videoX 인덱스를 추출
int resolveVideoIndexFromSymlink(const std::string& symlink) {
    namespace fs = std::filesystem;

    if (!fs::exists(symlink) || !fs::is_symlink(symlink)) {
        std::cerr << "[WARN] Symbolic link does not exist: " << symlink << std::endl;
        return -1;
    }

    fs::path target = fs::read_symlink(symlink);
    std::string filename = target.filename().string(); // "videoX"
    std::smatch match;
    std::regex re("video(\\d+)");
    if (std::regex_match(filename, match, re)) {
        return std::stoi(match[1]);
    }

    std::cerr << "[WARN] Failed to extract index from symlink target: " << filename << std::endl;
    return -1;
}

// videoX가 우리가 원하는 카메라인지 확인
bool isDesiredCamera(int index, const std::string& target_name) {
    std::string name_path = "/sys/class/video4linux/video" + std::to_string(index) + "/name";
    std::ifstream name_file(name_path);
    if (!name_file.is_open()) {
        return false;
    }
    std::string name;
    std::getline(name_file, name);
    return name.find(target_name) != std::string::npos;
}

// video0 ~ video9 중에서 원하는 카메라를 탐색
int findVideoIndexByName(const std::string& target_name, int max_index = 10) {
    for (int i = 0; i < max_index; ++i) {
        if (isDesiredCamera(i, target_name)) {
            std::cout << "[INFO] Found target camera at index " << i << std::endl;
            return i;
        }
    }
    return -1;
}

int main() {
    int index = resolveVideoIndexFromSymlink(SYMLINK_PATH);

    if (index == -1 || !isDesiredCamera(index, TARGET_CAMERA_NAME)) {
        std::cerr << "[WARN] video-cam not valid. Trying fallback..." << std::endl;
        index = findVideoIndexByName(TARGET_CAMERA_NAME);
        if (index == -1) {
            std::cerr << "[ERROR] No matching camera found." << std::endl;
            return -1;
        }
    }

    std::cout << "[INFO] Trying to open /dev/video" << index << std::endl;
    cv::VideoCapture cap(index);

    if (!cap.isOpened()) {
        std::cerr << "[ERROR] Failed to open /dev/video" << index << std::endl;
        return -1;
    }

    std::cout << "[INFO] Camera opened successfully." << std::endl;

    cv::Mat frame;
    int frame_count = 0;
    while (true) {
        if (!cap.read(frame)) {
            std::cerr << "[ERROR] Failed to capture frame." << std::endl;
            break;
        }
        std::cout << "[INFO] Captured frame #" << ++frame_count << std::endl;
      //  std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    return 0;
}
