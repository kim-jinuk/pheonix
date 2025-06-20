#include "running_control_csc/sender.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <thread>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <chrono>

#define MAX_PACKET_SIZE 1400
using boost::asio::ip::udp;
using namespace std::chrono;
constexpr uint32_t MAGIC = 0xDEADBEEF;
constexpr int HEADER_SIZE = 12; // Magic(4) + FrameID(4) + PacketID(2) + TotalPackets(2)
constexpr int OBJECTINFO_SIZE = 14;
constexpr int OBJECT_COUNT = 5;
constexpr int META_SIZE = OBJECTINFO_SIZE * OBJECT_COUNT; // 14 * 5 = 70
constexpr int PAYLOAD_OFFSET = HEADER_SIZE + META_SIZE;

/*
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
    */
// boos library
UdpSender::UdpSender(const std::string& ip, int port)
    : io_context_(), socket_(io_context_), endpoint_(boost::asio::ip::make_address(ip), port) {
    socket_.open(udp::v4());
    std::cout << "[ASIO] UDP async socket open to " << ip << ":" << port << "\n";
}

UdpSender::~UdpSender() {
    socket_.close();
}


std::vector<std::vector<uint8_t>> UdpSender::BuildUdpPackets(
    const FrameData& frame, const std::vector<ObjectInfo>& objs) 
{
    std::vector<std::vector<uint8_t>> packets;

    // 1. 이미지가 비어있다면 반환
    if (frame.img_bgr.empty()) return packets;

    // 2. JPEG 압축
    std::vector<uchar> jpeg_buf;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 60};  // 압축률 조정 가능
    auto t0 = steady_clock::now(); 
    bool success = cv::imencode(".jpg", frame.img_bgr, jpeg_buf, params);
    auto t1 = steady_clock::now(); 
    if (!success || jpeg_buf.empty()) return packets;
    // std::cout << "[INFO] JPEG 압축 시간: "
    //           << duration_cast<milliseconds>(t1 - t0).count() << " ms" << std::endl;
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


void UdpSender::HandleSend(const boost::system::error_code& ec, std::size_t bytes_transferred, int packet_id) {
    if (ec) {
        std::cerr << "[ERR] Packet " << packet_id << " failed: " << ec.message() << "\n";
    } else {
        // std::cout << "[ASIO] Packet " << packet_id << " sent (" << bytes_transferred << " bytes)\n";
    }
}

void UdpSender::UdpSend(const std::vector<std::vector<uint8_t>>& packets) {
    for (size_t i = 0; i < packets.size(); ++i) {
        socket_.async_send_to(
            boost::asio::buffer(packets[i]),
            endpoint_,
            boost::bind(&UdpSender::HandleSend, this, boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred, i)
        );

        // 최소 간격 sleep 또는 timer 없이도 괜찮음 (ASIO는 내부 큐 있음)
        std::this_thread::sleep_for(std::chrono::microseconds(50)); // tuning
    }

    io_context_.run();
    io_context_.reset();

   
}