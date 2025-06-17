#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <iostream>

#define DEST_IP   "192.168.1.2"
#define DEST_PORT 5000
#define MAX_FRAMES 100
// 20 ms
int main() {
    // MJPEG 요청 (OpenCV가 디코딩해서 BGR로 넘겨줌)
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);

    if (!cap.isOpened()) {
        std::cerr << "[ERR] Failed to open camera (V4L2 + MJPEG)\n";
        return -1;
    }

    // UDP 소켓 설정
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest.sin_addr);

    for (int quality : {30, 40, 50, 60, 70, 80, 90}) {
        std::cout << "\n=== Testing JPEG_QUALITY = " << quality << " ===\n";
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};

        for (int frame_id = 0; frame_id < MAX_FRAMES; ++frame_id) {
            auto t0 = std::chrono::high_resolution_clock::now();

            cv::Mat bgr_frame;
            //cap.read(bgr_frame);
            cap >> bgr_frame;  // 이미 BGR 상태로 반환됨
            if (bgr_frame.empty()) {
                std::cerr << "[WARN] Empty frame\n";
                continue;
            }

            auto t1 = std::chrono::high_resolution_clock::now();

            std::vector<uchar> jpeg;
            cv::imencode(".jpg", bgr_frame, jpeg, params);

            auto t2 = std::chrono::high_resolution_clock::now();

            if (jpeg.size() > 60000) {
                std::cerr << "[WARN] Frame too large: " << jpeg.size() << " bytes\n";
                continue;
            }

            sendto(sock, jpeg.data(), jpeg.size(), 0, (sockaddr*)&dest, sizeof(dest));
            auto t3 = std::chrono::high_resolution_clock::now();

            double read_time    = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double encode_time  = std::chrono::duration<double, std::milli>(t2 - t1).count();
            double send_time    = std::chrono::duration<double, std::milli>(t3 - t2).count();
            double total_time   = std::chrono::duration<double, std::milli>(t3 - t0).count();

        
            std::cout << "[Q" << quality << "] Frame " << frame_id
                      << " | Read: " << read_time << " ms"
                      << " | Encode: " << encode_time << " ms"
                      << " | Send: " << send_time << " ms"
                      << " | Total: " << total_time << " ms"
                      << " | Size: " << jpeg.size() << " bytes\n";
        }
    }

    close(sock);
    return 0;
}
