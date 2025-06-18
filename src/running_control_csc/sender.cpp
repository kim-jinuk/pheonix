#include "running_control_csc/sender.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include <vector>
#define MAX_PACKET_SIZE 1400


constexpr uint32_t MAGIC = 0xDEADBEEF;
constexpr int HEADER_SIZE = 12; // Magic(4) + FrameID(4) + PacketID(2) + TotalPackets(2)
constexpr int OBJECTINFO_SIZE = 14;
constexpr int OBJECT_COUNT = 5;
constexpr int META_SIZE = OBJECTINFO_SIZE * OBJECT_COUNT; // 14 * 5 = 70
constexpr int PAYLOAD_OFFSET = HEADER_SIZE + META_SIZE;


UdpSender::UdpSender(const std::string& ip, int port) {
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


std::vector<std::vector<uint8_t>> UdpSender::BuildUdpPackets(
    const FrameData& frame, const std::vector<ObjectInfo>& objs) 
{
    std::vector<std::vector<uint8_t>> packets;

    // 1. 이미지가 비어있다면 반환
    if (frame.img_bgr.empty()) return packets;

    // 2. JPEG 압축
    std::vector<uchar> jpeg_buf;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80};  // 압축률 조정 가능
    bool success = cv::imencode(".jpg", frame.img_bgr, jpeg_buf, params);
    if (!success || jpeg_buf.empty()) return packets;

    // 3. 패킷 수 계산
    size_t total_size = jpeg_buf.size();
    int payload_per_packet = MAX_PACKET_SIZE - PAYLOAD_OFFSET;
    int total_packets = (total_size + payload_per_packet - 1) / payload_per_packet;

    // 4. 패킷 생성
    for (int i = 0; i < total_packets; ++i) {
        int offset = i * payload_per_packet;
        int chunk_size = std::min((int)(total_size - offset), payload_per_packet);

        std::vector<uint8_t> packet(PAYLOAD_OFFSET + chunk_size);

        // Header (0 ~ 11)
        uint32_t magic_net = htonl(MAGIC);
        uint32_t fid_net = htonl(frame.frame_id); 
        uint16_t pid_net = htons(i);
        uint16_t tpkts_net = htons(total_packets);

        memcpy(&packet[0],  &magic_net,  4);
        memcpy(&packet[4],  &fid_net,    4);
        memcpy(&packet[8],  &pid_net,    2);
        memcpy(&packet[10], &tpkts_net,  2);

        // ObjectInfo (offset 12)
        for (int j = 0; j < OBJECT_COUNT; ++j) {
            ObjectInfo obj = (j < objs.size()) ? objs[j] : ObjectInfo{};
            memcpy(&packet[HEADER_SIZE + j * OBJECTINFO_SIZE], &obj, OBJECTINFO_SIZE);
        }

        // Payload (offset 12 + 70 = 82)
        memcpy(&packet[PAYLOAD_OFFSET], jpeg_buf.data() + offset, chunk_size);

        packets.push_back(std::move(packet));
    }

    return packets;
}


void UdpSender::UdpSend(const std::vector<std::vector<uint8_t>>& packets) {
    for (const auto& pkt : packets) {
        sendto(sock_, pkt.data(), pkt.size(), 0, (sockaddr*)&addr_, sizeof(addr_));
        usleep(1000);  // Optional pacing
    }
}