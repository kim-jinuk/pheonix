#ifndef UDPSENDER_HPP
#define UDPSENDER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <netinet/in.h>
#include <chrono>
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>


#define USEBOOST 1

#if USEBOOST ==1
    #include <boost/asio.hpp>  
#endif
class UdpSender {
public:
    UdpSender(const std::string& ip, int port);
    ~UdpSender();

    std::vector<std::vector<uint8_t>> BuildUdpPackets(const FrameData& frame, const std::vector<ObjectInfo>& objs); 
    void UdpSend(const std::vector<std::vector<uint8_t>>& packets);  
private:
#if USEBOOST == 1
    void HandleSend(const boost::system::error_code& ec, std::size_t bytes_transferred, int packet_id);
#endif
    int sock_;
    sockaddr_in addr_;
    std::atomic<int> sent_count_{0};
    int total_packet_count_ = 0;
    std::chrono::high_resolution_clock::time_point frame_start_time_;
#if USEBOOST == 1
    boost::asio::io_context io_context_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint endpoint_;
#endif
};

#endif