#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <vector>
#include <chrono>

#define LISTEN_PORT 5001
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480
#define FRAME_SIZE (FRAME_WIDTH * FRAME_HEIGHT * 3)

struct Packet {
    int frame_id;
    int packet_id;
    int total_packets;
    std::vector<uint8_t> data;
};

int main() {
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

    std::cout << "[INFO] Listening on port " << LISTEN_PORT << "...\n";

    std::map<int, std::vector<std::vector<uint8_t>>> frame_buffer;
    int last_displayed = -1;
    auto last_time = std::chrono::steady_clock::now();

    while (true) {
        std::vector<uint8_t> buf(65536);
        ssize_t len = recv(sock, buf.data(), buf.size(), 0);
        if (len < 12) {
            std::cerr << "[WARN] Packet too small: " << len << "\n";
            continue;
        }

        int frame_id, packet_id, total_packets;
        std::memcpy(&frame_id, buf.data(), 4);
        std::memcpy(&packet_id, buf.data() + 4, 4);
        std::memcpy(&total_packets, buf.data() + 8, 4);

        std::cout << "[RECV] Frame " << frame_id << " Packet " << packet_id
                  << "/" << total_packets << " (" << len << " bytes)\n";

        if (frame_id <= last_displayed) continue;

        auto& packets = frame_buffer[frame_id];
        if (packets.empty()) packets.resize(total_packets);
        packets[packet_id] = std::vector<uint8_t>(buf.begin() + 12, buf.begin() + len);

        bool complete = true;
        for (const auto& p : packets) {
            if (p.empty()) {
                complete = false;
                break;
            }
        }

        if (complete) {
            std::vector<uint8_t> full_data;
            for (const auto& p : packets)
                full_data.insert(full_data.end(), p.begin(), p.end());

            if (full_data.size() != FRAME_SIZE) {
                std::cerr << "[ERR] Frame size mismatch: got " << full_data.size()
                          << ", expected " << FRAME_SIZE << "\n";
                frame_buffer.erase(frame_id);
                continue;
            }

            // 안전한 복사 방식
            cv::Mat image(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3);
            std::memcpy(image.data, full_data.data(), FRAME_SIZE);

            auto now = std::chrono::steady_clock::now();
            double fps = 1000.0 / std::chrono::duration<double, std::milli>(now - last_time).count();
            last_time = now;

            cv::putText(image, "Frame: " + std::to_string(frame_id), {10, 30},
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0}, 2);
            cv::putText(image, "FPS: " + cv::format("%.2f", fps), {10, 60},
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,255}, 2);

            cv::imshow("Receiver", image);
            if (cv::waitKey(1) == 27) break;

            frame_buffer.erase(frame_id);
            last_displayed = frame_id;

            // 오래된 프레임 정리 (메모리 보호)
            for (auto it = frame_buffer.begin(); it != frame_buffer.end();) {
                if (it->first < frame_id - 10)
                    it = frame_buffer.erase(it);
                else
                    ++it;
            }
        }
    }

    close(sock);
    return 0;
}
