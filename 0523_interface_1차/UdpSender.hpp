
#ifndef UDPSENDER_HPP
#define UDPSENDER_HPP

#define UDP_MAGIC_WORD 0xDEADBEEF

#include <string>
#include <vector>
#include <netinet/in.h>

struct ObjectInfo {
    uint8_t cls;
    int16_t x, y, w, h;
    float conf;
};

class UdpSender {
    int sock;
    sockaddr_in addr;
    uint32_t frame_id;
    std::string ip;
    int port;

public:
    UdpSender(const std::string& ip, int port);
    ~UdpSender();

    void UdpInit();
    std::vector<uint8_t> UdpPayload();
    std::vector<uint8_t> UdpMeta(const std::vector<ObjectInfo>& objects, uint8_t nx, uint8_t ny);
    std::vector<uint8_t> UdpPacket(const std::vector<ObjectInfo>& objects, uint8_t nx, uint8_t ny);
    void UdpSend(const std::vector<uint8_t>& packet);
};

#endif
