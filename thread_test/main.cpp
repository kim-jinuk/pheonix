#include "UdpSender.hpp"
#include "CfgLoader.hpp"
#include "TcpReceiver.hpp"
#include "controlMotor.hpp"  

#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iostream>
#include <mutex>

std::atomic<bool> keep_running(true);
std::atomic<bool> tcp_connected(false);
std::atomic<int> current_mode(0);  // <-- 모드 공유 변수 추가

extern int angle_x;
extern int angle_y;
extern std::mutex angle_mutex;

void signal_handler(int) {
    keep_running = false;
}

// TCP 수신 스레드
void tcp_listener_thread(int port) {
    TcpReceiver receiver(port);

    while (keep_running) {
        std::cout << "[TCP] Waiting for client...\n";

        if (!receiver.AcceptConnection()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        tcp_connected = true;
        std::cout << "[TCP] Client connected.\n";

        TcpCommand cmd;
        while (keep_running) {
            if (!receiver.TcpParsing(cmd)) {
                std::cout << "[TCP] Client disconnected.\n";
                tcp_connected = false;
                {
                    std::lock_guard<std::mutex> lock(angle_mutex);
                    angle_x = 90;
                    angle_y = 90;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                break;
            }

            receiver.TcpAngle(cmd.dx, cmd.dy);
            current_mode = cmd.mode_num;  // <-- 모드 상태 저장

            if (cmd.mode_num != 1) {
                UpdateMotor(cmd.dx, cmd.dy);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    }
}

// 왕복 모터 제어 스레드
void motor_swing_thread() {
    const int dx = 2;
    const int delay_ms = 100;
    bool moving_left = true;

    while (keep_running) {
        if (current_mode != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int current_angle;
        {
            std::lock_guard<std::mutex> lock(angle_mutex);
            current_angle = angle_x;
        }

        if (moving_left) {
            if (current_angle <= 0) {
                moving_left = false;
                continue;
            }
            UpdateMotor(-dx, 0);
            std::cout << "[SWING] Left\n";
        } else {
            if (current_angle >= 180) {
                moving_left = true;
                continue;
            }
            UpdateMotor(dx, 0);
            std::cout << "[SWING] Right\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
}

// UDP 송신 스레드
void udp_sender_thread(const std::string& ip, int port) {
    UdpSender sender(ip, port);
    sender.UdpInit();

    while (keep_running) {
        while (keep_running && !tcp_connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        while (keep_running && tcp_connected) {
            sender.CaptureAndStorePayload();

            uint8_t nx, ny;
            {
                std::lock_guard<std::mutex> lock(angle_mutex);
                nx = angle_x;
                ny = angle_y;
            }

            auto packets = sender.BuildUdpPackets({}, nx, ny);
            sender.UdpSend(packets);
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        std::cout << "[UDP] Waiting for TCP reconnection...\n";
    }
}

int main() {
    InitMotor();
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
    std::thread motorSwingThread(motor_swing_thread); // <-- 왕복 스레드 실행

    tcpThread.join();
    udpThread.join();
    motorSwingThread.join();

    return 0;
}
