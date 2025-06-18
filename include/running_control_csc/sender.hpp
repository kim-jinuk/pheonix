#ifndef UDPSENDER_HPP
#define UDPSENDER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <netinet/in.h>
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>

class UdpSender {
public:
    UdpSender(const std::string& ip, int port);
    ~UdpSender();

    std::vector<std::vector<uint8_t>> BuildUdpPackets(const FrameData& frame, const std::vector<ObjectInfo>& objs); 
    void UdpSend(const std::vector<std::vector<uint8_t>>& packets);  
private:
    int sock_;
    sockaddr_in addr_;
};

#endif