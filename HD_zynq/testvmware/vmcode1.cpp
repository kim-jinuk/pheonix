#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <atomic>

#define TCP_MAGIC_WORD 0xA5A5

// 문자열 변환 함수
const char* modeToStr(int mode) {
    switch (mode) {
        case 0: return "SCAN";
        case 1: return "MANUAL";
        case 2: return "TRACKING";
        default: return "UNKNOWN";
    }
}

const char* stateToStr(int state) {
    switch (state) {
        case 0: return "CHECKING";
        case 1: return "IDLE";
        case 2: return "RUNNING";
        default: return "UNKNOWN";
    }
}

// 전역 소켓
int sock = -1;
std::atomic<bool> is_connected(false);

// 수신 쓰레드
void recv_thread() {
    while (is_connected.load()) {
        uint8_t buf[3];
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) {
            std::cout << "[HOST] Disconnected or recv error\n";
            is_connected.store(false);
            break;
        }

        // Magic word 체크
        uint16_t magic = (buf[0] << 8) | buf[1];
        if (magic != TCP_MAGIC_WORD) {
            std::cerr << "[HOST] Invalid magic word: " << std::hex << magic << std::dec << "\n";
            continue;
        }

        // payload 파싱
        uint8_t payload = buf[2];
        bool cam = payload & 0x01;
        bool tpu = payload & 0x02;
        int state = (payload >> 2) & 0x03;
        int mode  = (payload >> 4) & 0x03;

        std::cout << "[RECV] MODE: " << modeToStr(mode)
                  << ", STATE: " << stateToStr(state)
                  << ", TPU: " << (tpu ? "ON" : "OFF")
                  << ", CAM: " << (cam ? "ON" : "OFF") << "\n";
    }
}

// 전송 쓰레드: mode dx dy 입력 → 패킷 전송
void send_thread() {
    while (is_connected.load()) {
        std::string input;
        std::cout << "[SEND] 명령 입력 (mode dx dy): ";
        std::getline(std::cin, input);

        int mode, dx, dy;
        if (sscanf(input.c_str(), "%d %d %d", &mode, &dx, &dy) != 3) {
            std::cout << "[SEND] 잘못된 입력입니다. 예: 1 5 -3\n";
            continue;
        }

        uint8_t packet[6];
        packet[0] = (TCP_MAGIC_WORD >> 8) & 0xFF;
        packet[1] = TCP_MAGIC_WORD & 0xFF;
        packet[2] = static_cast<uint8_t>(mode);
        packet[3] = 0; // reserved
        packet[4] = static_cast<uint8_t>(dx);
        packet[5] = static_cast<uint8_t>(dy);

        ssize_t n = send(sock, packet, sizeof(packet), 0);
        if (n != sizeof(packet)) {
            std::cerr << "[SEND] 전송 실패\n";
            is_connected.store(false);
            break;
        }
    }
}

int main() {
    const char* server_ip = "192.168.1.3";
    const int server_port = 9999;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    std::cout << "[HOST] Connecting to server...\n";
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    std::cout << "[HOST] Connected!\n";
    is_connected.store(true);

    std::thread t_recv(recv_thread);
    std::thread t_send(send_thread);

    t_recv.join();
    t_send.join();

    close(sock);
    return 0;
}
