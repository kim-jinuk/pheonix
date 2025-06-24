#include "running_control_csc/sender.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using boost::asio::ip::udp;
using namespace std::chrono;

//--------------------------------------------------
// Define
//--------------------------------------------------
#define MAX_PACKET_SIZE 1400
constexpr uint32_t MAGIC = 0xDEADBEEF;
constexpr int HEADER_SIZE = 12; // Magic(4) + FrameID(4) + PacketID(2) + TotalPackets(2)
constexpr int OBJECTINFO_SIZE = 14;
constexpr int OBJECT_COUNT = 5;
constexpr int META_SIZE = OBJECTINFO_SIZE * OBJECT_COUNT; // 14 * 5 = 70
constexpr int TIMESTAMP_SIZE = 23;
constexpr int PAYLOAD_OFFSET = HEADER_SIZE + META_SIZE + TIMESTAMP_SIZE;

UdpSender::UdpSender(const std::string& ip, int port)
    : io_context_(), socket_(io_context_), endpoint_(boost::asio::ip::make_address(ip), port) {
    socket_.open(udp::v4());
  //  std::cout << "[ASIO] UDP async socket open to " << ip << ":" << port << "\n";
}

UdpSender::~UdpSender() {
    socket_.close();
}

std::vector<std::vector<uint8_t>> UdpSender::BuildUdpPackets(
    const FrameData& frame, const std::vector<ObjectInfo>& objs) 
{
    std::vector<std::vector<uint8_t>> packets;

    if (frame.img_bgr.empty()) return packets;

    std::vector<uchar> jpeg_buf;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 60};

    bool success = cv::imencode(".jpg", frame.img_bgr, jpeg_buf, params);
    if (!success || jpeg_buf.empty()) return packets;

    //  JPEG 버퍼 크기 출력
    size_t total_size = jpeg_buf.size();
   // std::cout << "[DBG] jpeg_buf size: " << total_size << std::endl;

    const int ts_len = TIMESTAMP_SIZE;
    const int payload_per_packet = MAX_PACKET_SIZE - PAYLOAD_OFFSET;
    int total_packets = (total_size + payload_per_packet - 1) / payload_per_packet;

    //  패킷 개수 출력
  //  std::cout << "[DBG] total_packets: " << total_packets << std::endl;

    const std::string& ts = frame.timestamp;

    for (int i = 0; i < total_packets; ++i) {
        int offset = i * payload_per_packet;
        int chunk_size = std::min((int)(total_size - offset), payload_per_packet);

        //  루프당 현재 패킷 번호 및 기본 정보 출력
     //   std::cout << "[DBG] packet num: " << i 
     //             << ", offset: " << offset 
     //             << ", chunk_size: " << chunk_size << std::endl;

        int packet_size = PAYLOAD_OFFSET + chunk_size;

        //  패킷 메모리 크기와 쓰기 위치 비교
    //    std::cout << "[DBG] packet_size: " << packet_size 
    //              << ", payload_end: " << PAYLOAD_OFFSET + chunk_size << std::endl;

        if (packet_size < PAYLOAD_OFFSET + chunk_size) {
          //  std::cerr << "[ERR] packet buffer too small!" << std::endl;
        }

        std::vector<uint8_t> packet(packet_size);

        uint32_t magic_net = htonl(MAGIC);
        uint32_t fid_net = htonl(frame.frame_id); 
        uint16_t pid_net = htons(i);
        uint16_t tpkts_net = htons(total_packets);

        memcpy(&packet[0],  &magic_net,  4);
        memcpy(&packet[4],  &fid_net,    4);
        memcpy(&packet[8],  &pid_net,    2);
        memcpy(&packet[10], &tpkts_net,  2);

        for (int j = 0; j < OBJECT_COUNT; ++j) {
            ObjectInfo obj = (j < objs.size()) ? objs[j] : ObjectInfo{};
            memcpy(&packet[HEADER_SIZE + j * OBJECTINFO_SIZE], &obj, OBJECTINFO_SIZE);
        }

        memcpy(&packet[HEADER_SIZE + OBJECT_COUNT * OBJECTINFO_SIZE], ts.data(), ts_len);

        memcpy(&packet[PAYLOAD_OFFSET], jpeg_buf.data() + offset, chunk_size);

        packets.push_back(std::move(packet));
    }

    return packets;
}


void UdpSender::HandleSend(const boost::system::error_code& ec, std::size_t bytes_transferred, int packet_id) {
    if (ec) {
       // std::cerr << "[ERR] Packet " << packet_id << " failed: " << ec.message() << "\n";
    }
 /*  int finished = ++sent_count_;
   if (finished == total_packet_count_) {
        auto frame_end_time = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       frame_end_time - frame_start_time_).count();


        std::cout << " Frame sent completely. Time: " << duration_ms << " ms\n";
    }*/
}

void UdpSender::UdpSend(const std::vector<std::vector<uint8_t>>& packets) {
    total_packet_count_ = packets.size();
    sent_count_ = 0;
    frame_start_time_ = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < packets.size(); ++i) {
        socket_.async_send_to(
            boost::asio::buffer(packets[i]),
            endpoint_,
            boost::bind(&UdpSender::HandleSend, this, boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred, i)
        );

        std::this_thread::sleep_for(std::chrono::microseconds(50)); //50 ->GUI fps 20 stable
    }

    io_context_.run();
    io_context_.reset();
}