#include "UdpSender.hpp"
#include "CfgLoader.hpp"
#include "TcpReceiver.hpp"  // 추가

#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iostream>

std::atomic<bool> keep_running(true);
std::atomic<bool> tcp_connected(false);

void signal_handler(int) {
    keep_running = false;
}

// TCP 수신 스레드 (TcpReceiver 클래스를 사용)
void tcp_listener_thread(int port) {
    TcpReceiver receiver(port);
    if (!receiver.AcceptConnection()) return;

    tcp_connected = true;

    TcpCommand cmd;
    while (keep_running) {
        if (!receiver.TcpParsing(cmd)) break;  // 클라이언트 종료
        receiver.TcpAngle(cmd.dx, cmd.dy);     // 명령 처리
    }

    tcp_connected = false;
    std::cout << "[TCP] Client disconnected.\n";
}

// UDP 송신 스레드 (변경 없음)
void udp_sender_thread(const std::string& ip, int port) {
    UdpSender sender(ip, port);
    sender.UdpInit();

    while (keep_running) {
        while (keep_running && !tcp_connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        while (keep_running && tcp_connected) {
            sender.CaptureAndStorePayload();
            auto packets = sender.BuildUdpPackets({}, 0, 0);
            sender.UdpSend(packets);
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        std::cout << "[UDP] Waiting for TCP reconnection...\n";
    }
}

int main() {

    std::cout << "sizeof(ObjectInfo): " << sizeof(ObjectInfo) << std::endl;


    
    std::signal(SIGINT, signal_handler);

    CfgLoader cfg;
    if (!cfg.load("config.cfg")) {
        std::cerr << "[ERR] Failed to load config.cfg\n";
        return 1;
    }

    std::string udp_ip = cfg.get("UDP_IP");
    int udp_port = std::stoi(cfg.get("UDP_PORT"));
    int tcp_port = std::stoi(cfg.get("TCP_PORT"));

    std::cout << "[MAIN] Config loaded: UDP(" << udp_ip << ":" << udp_port << "), TCP(" << tcp_port << ")\n";

    std::thread tcpThread(tcp_listener_thread, tcp_port);
    std::thread udpThread(udp_sender_thread, udp_ip, udp_port);

    tcpThread.join();
    udpThread.join();

    return 0;
}
