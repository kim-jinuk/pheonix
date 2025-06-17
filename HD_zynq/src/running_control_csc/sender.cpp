#include "running_control_csc/sender.hpp"
#include "running_control_csc/videoCapture.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <chrono>   // 추가: 시간 측정용
#include <cerrno>   // errno
#define MAX_PACKET_SIZE 1400

// 상수 정의 (UdpSender.cpp 상단 또는 함수 내부에)
constexpr uint32_t MAGIC = 0xDEADBEEF;
constexpr int HEADER_SIZE = 12; // Magic(4) + FrameID(4) + PacketID(2) + TotalPackets(2)
constexpr int OBJECTINFO_SIZE = 14;
constexpr int OBJECT_COUNT = 5;
constexpr int META_SIZE = OBJECTINFO_SIZE * OBJECT_COUNT; // 14 * 5 = 70
constexpr int PAYLOAD_OFFSET = HEADER_SIZE + META_SIZE;

UdpSender::UdpSender(const std::string& ip, int port) : frame_id(0) {
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    std::cout << "[UDP] sock craete()"<< "\n";
    if (sock_ < 0) {
        perror("UDP socket");
        exit(1);
    }

    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
}

UdpSender::~UdpSender() {
    close(sock_);
}

void UdpSender::UdpInit() {
    frame_id = 0;
    std::cout << "[UDP] UdpInit()"<< "\n";
}
// 이미지 받아오는 거
void UdpSender::CaptureAndStorePayload() {

   payload_ = CaptureFrame();
  // std::cout << "[UDP] CaptureFrame()"<< "\n";
}   

std::vector<std::vector<uint8_t>> UdpSender::BuildUdpPackets(const std::vector<ObjectInfo>& objs, uint8_t nx, uint8_t ny) {
    std::vector<std::vector<uint8_t>> packets;
    if (payload_.empty()) return packets;

    size_t total_size = payload_.size();
    int payload_per_packet = MAX_PACKET_SIZE - PAYLOAD_OFFSET;
    int total_packets = (total_size + payload_per_packet - 1) / payload_per_packet;

    for (int i = 0; i < total_packets; ++i) {
        int offset = i * payload_per_packet;
        int chunk_size = std::min((int)(total_size - offset), payload_per_packet);

        std::vector<uint8_t> packet(PAYLOAD_OFFSET + chunk_size);

        // Header (0 ~ 11)
        uint32_t magic_net = htonl(MAGIC);
        uint32_t fid_net = htonl(frame_id);
        uint16_t pid_net = htons(i);
        uint16_t tpkts_net = htons(total_packets);

        memcpy(&packet[0],  &magic_net,  4);
        memcpy(&packet[4],  &fid_net,    4);
        memcpy(&packet[8],  &pid_net,    2);
        memcpy(&packet[10], &tpkts_net,  2);

        // ObjectInfo 5개 고정 전송 (offset 12)
        for (int j = 0; j < OBJECT_COUNT; ++j) {
            ObjectInfo obj = (j < objs.size()) ? objs[j] : ObjectInfo{};
            memcpy(&packet[HEADER_SIZE + j * OBJECTINFO_SIZE], &obj, OBJECTINFO_SIZE);
        }

        // Payload 붙이기
        memcpy(&packet[PAYLOAD_OFFSET], payload_.data() + offset, chunk_size);

        packets.push_back(std::move(packet));
    }

    return packets;
}


void UdpSender::UdpSend(const std::vector<std::vector<uint8_t>>& packets) {
    // for (const auto& pkt : packets) {
    //     ssize_t sent = sendto(sock_, pkt.data(), pkt.size(), 0, (sockaddr*)&addr_, sizeof(addr_));
    //     if (sent < 0) {
    //         perror("[ERR] sendto failed");
    //     }
    //     usleep(10000);  // Fine-tuned pacing
    //     //usleep(100);
    //      std::cerr << "[ERRNO] packet " << i << " errno: " << errno << " - " << strerror(errno) << "\n";
    //         send_error = true;
    // }
    //  if (!send_error) {
    //     std::cout << "[UDP] Frame " << frame_id << " sent OK with " << packets.size() << " packets\n";
    // } else {
    //     std::cout << "[UDP] Frame " << frame_id << " had send errors!\n";
    // }
    // usleep(100000); // Ensure receiver processes whole frame
    // frame_id++; 
    
    // std::cout << "[UDP] Frame " << frame_id << " sent with " << packets.size() << " packets\n";

    bool send_error = false;

    for (size_t i = 0; i < packets.size(); ++i) {
        const auto& pkt = packets[i];
        auto t1 = std::chrono::high_resolution_clock::now();
        ssize_t sent = sendto(sock_, pkt.data(), pkt.size(), 0, (sockaddr*)&addr_, sizeof(addr_));
        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        double delta_ms = duration.count() / 1000.0;
        if (sent < 0) {
            perror("[ERR] sendto failed");
            std::cerr << "[ERRNO] packet " << i
                      << " errno: " << errno
                      << " - " << strerror(errno) << "\n";
            send_error = true;
        }
        else {
            // std::cout << "[SEND] Frame " << frame_id
            //       << " Packet " << i << " sent, took " << delta_ms << " ms\n";
        }
        usleep(100);  // Fine-tuned pacing
      // usleep(1000);
    }

    if (!send_error) {
        std::cout << "[UDP] rame " << frame_id
                  << " sent OK with " << packets.size() << " packets\n";
    } else {
        std::cout << "[UDP] Frame " << frame_id
                  << " had send errors!\n";
    }

 //   usleep(100000); // Ensure receiver processes whole frame
    frame_id++;
}