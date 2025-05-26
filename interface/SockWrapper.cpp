
#include "SockWrapper.hpp"
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>

int SockWrapper::create_udp_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) perror("UDP socket");
    return sock;
}

int SockWrapper::create_tcp_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) perror("TCP socket");
    return sock;
}

void SockWrapper::close_socket(int sock) {
    close(sock);
}

sockaddr_in SockWrapper::make_addr(const std::string& ip, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    return addr;
}
