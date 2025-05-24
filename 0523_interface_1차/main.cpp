#include "UdpSender.hpp"
#include "TcpReceiver.hpp"
#include "CfgLoader.hpp"
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<bool> tcp_connected(false);
std::atomic<int8_t> current_dx(0);
std::atomic<int8_t> current_dy(0);

std::string g_udp_ip;
int g_udp_port;
void tcp_thread_func(int tcp_port) {
    TcpReceiver receiver(tcp_port);

    if (!receiver.AcceptConnection()) return;

    tcp_connected = true;
    std::cout << "[TCP] Client connected.\n";

    // 이 시점에 UDP Sender 시작
    UdpSender sender(g_udp_ip, g_udp_port);
    sender.UdpInit();

    std::thread udp_thread([&sender]() {
        while (tcp_connected) {
            std::vector<ObjectInfo> objs = {
                {1, 100, 120, 40, 60, 0.95f},
                {2, 200, 150, 50, 70, 0.90f}
            };

            uint8_t nx = static_cast<uint8_t>(90 + current_dx);
            uint8_t ny = static_cast<uint8_t>(100 + current_dy);

            auto packet = sender.UdpPacket(objs, nx, ny);
            sender.UdpSend(packet);

            std::cout << "[UDP] Packet sent. nx=" << (int)nx << ", ny=" << (int)ny << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    TcpCommand cmd;
    while (receiver.TcpParsing(cmd)) {
        current_dx = cmd.dx;
        current_dy = cmd.dy;
        receiver.TcpAngle(cmd.dx, cmd.dy);
    }

    // 먼저 UDP 스레드 종료 대기 → 이후 TCP 상태 false 처리
    udp_thread.join();
    tcp_connected = false;

    std::cout << "[TCP] Connection closed." << std::endl;
}


int main() {
    CfgLoader cfg;
    if (!cfg.load("config.cfg")) {
        std::cerr << "[ERR] Failed to load config.cfg" << std::endl;
        return 1;
    }

    g_udp_ip = cfg.get("UDP_IP");
    g_udp_port = std::stoi(cfg.get("UDP_PORT"));
    int tcp_port = std::stoi(cfg.get("TCP_PORT"));

    std::thread tcp_thread(tcp_thread_func, tcp_port);
    tcp_thread.join();

    return 0;
}
