#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <chrono>

#define LISTEN_PORT 5000

int main() {
    // UDP 소켓 생성
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(LISTEN_PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind");
        return -1;
    }

    std::vector<uchar> buf(65536);  // 최대 수신 버퍼
    int frame_id = 0;
    auto last_time = std::chrono::steady_clock::now();

    while (true) {
        ssize_t len = recv(sock, buf.data(), buf.size(), 0);
        if (len <= 0) continue;

        // JPEG 데이터를 Mat로 wrap
        cv::Mat jpeg_data(1, len, CV_8UC1, buf.data());
        cv::Mat image = cv::imdecode(jpeg_data, cv::IMREAD_COLOR);
        if (image.empty()) {
            std::cerr << "[WARN] Failed to decode image\n";
            continue;
        }

        // FPS 계산
        auto now = std::chrono::steady_clock::now();
        double fps = 1000.0 / std::chrono::duration<double, std::milli>(now - last_time).count();
        last_time = now;

        // 오버레이
        cv::putText(image, "Frame: " + std::to_string(frame_id++), {10, 30},
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, {0, 255, 0}, 2);
        cv::putText(image, "FPS: " + cv::format("%.2f", fps), {10, 60},
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, {0, 255, 255}, 2);

        cv::imshow("Receiver", image);
        if (cv::waitKey(1) == 27) break; // ESC 키로 종료
    }

    close(sock);
    return 0;
}
