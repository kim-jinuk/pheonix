#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstring>

#define DEST_IP "192.168.1.2"
#define DEST_PORT 5001
#define MAX_PACKET_SIZE 60000
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

int main() {
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));

    if (!cap.isOpened()) {
        std::cerr << "Camera open failed\n";
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest.sin_addr);

    int frame_id = 0;

    while (true) {
        auto t_start = std::chrono::high_resolution_clock::now();

        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) continue;

        int payload_size = frame.total() * frame.elemSize(); // BGR 8bit
        int packet_count = (payload_size + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;

        for (int i = 0; i < packet_count; ++i) {
            int offset = i * MAX_PACKET_SIZE;
            int chunk_size = std::min(MAX_PACKET_SIZE, payload_size - offset);

            std::vector<uint8_t> packet(12 + chunk_size);  // header(12) + data
            std::memcpy(packet.data(), &frame_id, 4);        // frame ID
            std::memcpy(packet.data() + 4, &i, 4);           // packet ID
            std::memcpy(packet.data() + 8, &packet_count, 4);// total packets
            std::memcpy(packet.data() + 12, frame.data + offset, chunk_size);

            sendto(sock, packet.data(), packet.size(), 0, (sockaddr*)&dest, sizeof(dest));
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        std::cout << "[Sender] Frame " << frame_id++ << ", sent in " << ms << " ms\n";
    }

    close(sock);
    return 0;
}
