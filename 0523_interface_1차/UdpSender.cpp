
#include "UdpSender.hpp"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>

UdpSender::UdpSender(const std::string& ip, int port)
    : ip(ip), port(port), frame_id(1) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
}

UdpSender::~UdpSender() {
    close(sock);
}

void UdpSender::UdpInit() {
    frame_id = 1;
}

std::vector<uint8_t> UdpPayload() {
    // 미구현 (필요 시 이미지 버퍼)
    return {};
}

std::vector<uint8_t> UdpSender::UdpMeta(const std::vector<ObjectInfo>& objects, uint8_t nx, uint8_t ny) {
    std::vector<uint8_t> meta;
    meta.resize(84);  // 84 = 4(magic) + 4(frame_id) + 2(packet_id) + 2(total_packets) + 1(nx) + 1(ny) + 70(ObjectInfo*5)

    uint32_t magic = htonl(UDP_MAGIC_WORD);
    std::memcpy(&meta[0], &magic, 4);

    uint32_t fid = htonl(frame_id++);
    std::memcpy(&meta[4], &fid, 4);

    uint16_t pid = htons(1);
    uint16_t total = htons(1);
    std::memcpy(&meta[8], &pid, 2);
    std::memcpy(&meta[10], &total, 2);

    meta[12] = nx;
    meta[13] = ny;

    for (size_t i = 0; i < 5; ++i) {
        ObjectInfo obj = (i < objects.size()) ? objects[i] : ObjectInfo{0, 0, 0, 0, 0, 0.0f};
        std::memcpy(&meta[14 + i * sizeof(ObjectInfo)], &obj, sizeof(ObjectInfo));
    }

    return meta;
}

std::vector<uint8_t> UdpSender::UdpPacket(const std::vector<ObjectInfo>& objects, uint8_t nx, uint8_t ny) {
    return UdpMeta(objects, nx, ny);
}

void UdpSender::UdpSend(const std::vector<uint8_t>& packet) {
    sendto(sock, packet.data(), packet.size(), 0, (sockaddr*)&addr, sizeof(addr));
}
