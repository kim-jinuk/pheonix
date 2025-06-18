#ifndef UDPSENDER_HPP
#define UDPSENDER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <netinet/in.h>
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>

#include <boost/asio.hpp>  


class UdpSender {
public:
    UdpSender(const std::string& ip, int port);
    ~UdpSender();

    std::vector<std::vector<uint8_t>> BuildUdpPackets(const FrameData& frame, const std::vector<ObjectInfo>& objs); 
    void UdpSend(const std::vector<std::vector<uint8_t>>& packets);  
private:
    void HandleSend(const boost::system::error_code& ec, std::size_t bytes_transferred, int packet_id);

    int sock_;
    sockaddr_in addr_;

    boost::asio::io_context io_context_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint endpoint_;
};

#endif