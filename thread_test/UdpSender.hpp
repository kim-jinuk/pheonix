#ifndef UDPSENDER_HPP
#define UDPSENDER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <netinet/in.h>

struct __attribute__((packed)) ObjectInfo {
    uint16_t cls;
    uint16_t x, y, w, h;
    float conf;
};

class UdpSender {
public:
    UdpSender(const std::string& ip, int port);
    ~UdpSender();

    void UdpInit();  // frame_id 초기화
    void CaptureAndStorePayload();  // JPEG 프레임 캡처 후 내부 저장
    std::vector<std::vector<uint8_t>> BuildUdpPackets(const std::vector<ObjectInfo>& objs, uint8_t nx, uint8_t ny);  // 멀티 패킷 구성
    void UdpSend(const std::vector<std::vector<uint8_t>>& packets);  // UDP 전송 + frame_id 증가

private:
    uint32_t frame_id;
    int sock_;
    sockaddr_in addr_;
    std::vector<uint8_t> payload_;
};

#endif
