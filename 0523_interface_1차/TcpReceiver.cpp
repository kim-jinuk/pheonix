
#include "TcpReceiver.hpp"
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>

TcpReceiver::TcpReceiver(int port) {
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("TCP socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("TCP bind");
        exit(1);
    }

    if (listen(server_sock, 1) < 0) {
        perror("TCP listen");
        exit(1);
    }

    std::cout << "[TCP] Server ready and listening on port " << port << std::endl;
}

TcpReceiver::~TcpReceiver() {
    close(client_sock);
    close(server_sock);
}

bool TcpReceiver::AcceptConnection() {
    client_len = sizeof(client_addr);
    client_sock = accept(server_sock, (sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("TCP accept");
        return false;
    }
    std::cout << "[TCP] Client connected!" << std::endl;
    return true;
}

bool TcpReceiver::TcpParsing(TcpCommand& cmd) {
    uint8_t buf[6];
    ssize_t n = recv(client_sock, buf, 6, 0);
    if (n != 6) return false;

    uint16_t magic = (buf[0] << 8) | buf[1];
    if (magic != TCP_MAGIC_WORD) return false;

    cmd.mode_num = buf[2];
    cmd.dx = static_cast<int8_t>(buf[3]);
    cmd.dy = static_cast<int8_t>(buf[4]);
    cmd.tracking_id = static_cast<int8_t>(buf[5]);

    return true;
}

void TcpReceiver::TcpAngle(int8_t dx, int8_t dy) {
    std::cout << "[TCP] dx: " << static_cast<int>(dx)
              << ", dy: " << static_cast<int>(dy) << std::endl;
}
