#include "running_control_csc/receiver.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

using namespace std;

TcpReceiver::TcpReceiver(int port) : server_sock(-1), client_sock(-1), client_len(sizeof(client_addr)) {
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[ERR] socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[ERR] bind");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 1) < 0) {
        perror("[ERR] listen");
        close(server_sock);
        exit(1);
    }

    cout << "[TCP] Server listening on port " << port << "\n";
}

TcpReceiver::~TcpReceiver() {
    if (client_sock != -1) close(client_sock);
    if (server_sock != -1) close(server_sock);
}

bool TcpReceiver::AcceptConnection() {
    client_sock = accept(server_sock, (sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("[ERR] accept");
        return false;
    }
    cout << "[TCP] Client connected\n";
    return true;
}

bool TcpReceiver::TcpParsing(TcpCommand& cmd) {
    uint8_t buf[6];
    ssize_t n = recv(client_sock, buf, sizeof(buf), 0);
    if (n != sizeof(buf)) {
        cout << "[TCP] Client disconnected or invalid packet\n";
        close(client_sock);
        client_sock = -1;
        return false;
    }

    uint16_t magic = (buf[0] << 8) | buf[1];
    if (magic != TCP_MAGIC_WORD) {
        cerr << "[TCP] Invalid magic word\n";
        return true; // 연결은 유지하되 skip
    }

    cmd.mode_num = buf[2];
    cmd.dx = static_cast<int8_t>(buf[4]);
    cmd.dy = static_cast<int8_t>(buf[5]);

    return true;
}

void TcpReceiver::TcpAngle(int8_t dx, int8_t dy) {
    cout << "[RCV] dx=" << (int)dx << ", dy=" << (int)dy << "\n";
    // 실제 angle 제어는 외부에서 처리
}


bool TcpReceiver::sendState(bool tpu, bool cam, int state, int mode) {
    if (client_sock < 0)
        return false;

    
    uint8_t payload = 0;
    payload |= (static_cast<uint8_t>(mode)  & 0x03) << 4; // Bit 5~4
    payload |= (static_cast<uint8_t>(state) & 0x03) << 2; // Bit 3~2
    payload |= (tpu ? 0x02 : 0x00); // Bit 1
    payload |= (cam ? 0x01 : 0x00); // Bit 0

    uint8_t buf[3];
    buf[0] = (TCP_MAGIC_WORD >> 8) & 0xFF; // 0xA5
    buf[1] = TCP_MAGIC_WORD & 0xFF;        // 0xA5
    buf[2] = payload;

    if (send(client_sock, buf, sizeof(buf), 0) < 0 ) {
        close(client_sock);
        client_sock = -1;
        return false;
    }
    
    return true;
}