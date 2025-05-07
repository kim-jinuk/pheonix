#include <iostream>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>

extern int8_t current_angle_x;
extern int8_t current_angle_y;
extern uint16_t prev_mode;

/**
 * @brief TCP 서버 실행 함수
 * @details
 *  - 포트 9999에서 클라이언트 요청을 수신
 *  - 모드 전환 또는 모터 이동 명령을 처리하고 응답 전송
 *  - 명령은 5바이트의 구조로 구성되며, magic word, switch flag, 데이터로 이루어짐
 * 
 * @throws socket.error, bind.error, accept.error 등 시스템 소켓 오류 발생 가능
 */
void tcp_server() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr{}, client_addr{};
    socklen_t client_addr_size;
    uint8_t buffer[1024];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "TCP 소켓 생성 실패" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9999);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "TCP 바인드 실패" << std::endl;
        close(server_fd);
        return;
    }

    if (listen(server_fd, 5) < 0) {
        std::cerr << "TCP 리스닝 실패" << std::endl;
        close(server_fd);
        return;
    }

    std::cout << "TCP 서버 대기 중... 포트: 9999" << std::endl;

    client_addr_size = sizeof(client_addr);
    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_size);
    if (client_fd < 0) {
        std::cerr << "TCP 클라이언트 연결 수락 실패" << std::endl;
        close(server_fd);
        return;
    }

    std::cout << "TCP 클라이언트 연결됨" << std::endl;

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int len = read(client_fd, buffer, 5);
        if (len <= 0) {
            std::cout << "TCP 클라이언트 연결 종료." << std::endl;
            break;
        }

        uint16_t magic_word = buffer[0] | (buffer[1] << 8);
        uint8_t switch_flag = buffer[2];

        if (magic_word != 0xA5A5) {
            std::cout << "잘못된 magic word 수신: " << std::hex << magic_word << std::endl;
            continue;
        }

        /**
         * @brief 모드 전환 처리
         * @details switch_flag == 1 일 때 mode_num 수신
         *          - 이전 모드가 0이고 새 모드가 1 또는 2일 경우 각도 정보 포함 응답
         *          - 그 외에는 mode_num 그대로 echo-back 응답
         */
        if (switch_flag == 1 && len == 5) {
            uint16_t mode_num = buffer[3] | (buffer[4] << 8);
            std::cout << "[수신] 모드 전환 명령: mode_num = " << mode_num << std::endl;

            if (prev_mode == 0 && (mode_num == 1 || mode_num == 2)) {
                uint8_t resp[5] = {0x5A, 0x5A, switch_flag,
                                   static_cast<uint8_t>(current_angle_x),
                                   static_cast<uint8_t>(current_angle_y)};
                send(client_fd, resp, 5, 0);
                std::cout << "[응답] 현재 각도 전송: x = " << (int)current_angle_x
                          << ", y = " << (int)current_angle_y << std::endl;
            } else {
                uint8_t resp[5] = {0x5A, 0x5A, switch_flag,
                                   static_cast<uint8_t>(mode_num & 0xFF),
                                   static_cast<uint8_t>((mode_num >> 8) & 0xFF)};
                send(client_fd, resp, 5, 0);
                std::cout << "[응답] 모드 전환 완료" << std::endl;
            }

            prev_mode = mode_num;
        }

        /**
         * @brief 모터 이동 처리
         * @details switch_flag == 0 일 때 dx, dy 수신
         *          - 현재 각도 갱신 후 응답으로 angle 전송
         */
        else if (switch_flag == 0 && len == 5) {
            int8_t dx = static_cast<int8_t>(buffer[3]);
            int8_t dy = static_cast<int8_t>(buffer[4]);

            current_angle_x = std::clamp(current_angle_x + dx, -90, 90);
            current_angle_y = std::clamp(current_angle_y + dy, -90, 90);

            std::cout << "[수신] 모터 이동 명령: dx = " << (int)dx << ", dy = " << (int)dy;
            std::cout << " → 현재 각도: x = " << (int)current_angle_x
                      << ", y = " << (int)current_angle_y << std::endl;

            uint8_t resp[5] = {0x5A, 0x5A, switch_flag,
                               static_cast<uint8_t>(current_angle_x),
                               static_cast<uint8_t>(current_angle_y)};
            send(client_fd, resp, 5, 0);
        }

        else {
            std::cout << "데이터 길이 또는 switch_flag 오류" << std::endl;
        }
    }

    close(client_fd);
    close(server_fd);
}
