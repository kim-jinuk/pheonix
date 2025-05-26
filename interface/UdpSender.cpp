#include "UdpSender.hpp"
#include "videoCapture.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

#define MAX_PACKET_SIZE 1400

// 상수 정의 (UdpSender.cpp 상단 또는 함수 내부에)
constexpr int HEADER_SIZE = 8;
constexpr int NUM_OBJECTS = 5;
constexpr int META_SIZE = 2 + sizeof(ObjectInfo) * NUM_OBJECTS;
constexpr int PAYLOAD_OFFSET = HEADER_SIZE + META_SIZE;


UdpSender::UdpSender(const std::string& ip, int port) : frame_id(0) {
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
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
}

void UdpSender::CaptureAndStorePayload() {
    payload_ = CaptureFrame();
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

        // Header
        uint32_t fid_net = htonl(frame_id);
        uint16_t pid_net = htons(i);
        uint16_t tpkts_net = htons(total_packets);
        memcpy(&packet[0], &fid_net, 4);
        memcpy(&packet[4], &pid_net, 2);
        memcpy(&packet[6], &tpkts_net, 2);

        // nx, ny
        packet[8] = nx;
        packet[9] = ny;

        // ObjectInfo 5개 고정
        for (int j = 0; j < NUM_OBJECTS; ++j) {
            ObjectInfo obj = (j < objs.size()) ? objs[j] : ObjectInfo{};
            memcpy(&packet[10 + j * sizeof(ObjectInfo)], &obj, sizeof(ObjectInfo));
        }

        // Payload
        memcpy(&packet[PAYLOAD_OFFSET], payload_.data() + offset, chunk_size);

        packets.push_back(std::move(packet));
    }

    return packets;
}


void UdpSender::UdpSend(const std::vector<std::vector<uint8_t>>& packets) {
    for (const auto& pkt : packets) {
        sendto(sock_, pkt.data(), pkt.size(), 0, (sockaddr*)&addr_, sizeof(addr_));
        usleep(1000);  // Optional pacing
    }
    frame_id++;
}
