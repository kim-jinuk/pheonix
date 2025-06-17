#include "running_control_csc/receiver.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

using namespace std;

TcpBase::TcpBase(int port) : server_sock(-1), client_sock(-1), client_len(sizeof(client_addr)) {
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

TcpBase::~TcpBase() {
    if (client_sock != -1) close(client_sock);
    if (server_sock != -1) close(server_sock);
}

bool TcpBase::AcceptConnection() {

   client_sock = accept(server_sock, (sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("[ERR] accept");
        return false;
    }
    cout << "[TCP] Client connected\n";
    return true;
}

TcpCmdChannel::TcpCmdChannel(int port) : TcpBase(port) {}

bool TcpCmdChannel::TcpParsing(TcpCommand& cmd) {
    TcpCommand tmp;
    ssize_t n = recv(client_sock, &tmp, sizeof(tmp), 0);
    std::cout << std::hex << tmp.magic_word << std::endl;
    
    if (n != sizeof(tmp)) {
        cout << "[TCP] Client disconnected or invalid packet\n";
        close(client_sock);
        client_sock=-1;
        return false;
    }

    if (tmp.magic_word!=TCP_MAGIC_WORD) {
        cerr << "[TCP] Invalid magic word\n";
        return true; // 연결은 유지하되 skip
    }
    cmd=tmp;
    return true;
}

bool TcpCmdChannel::sendAck(TcpCommand&  cmd) {
    if (client_sock<0){
        return false;
    }
    
    if (send(client_sock,&cmd,sizeof(cmd),0)<0) {
        close(client_sock);
        client_sock=-1;
        return false;
    }
    
    return true;
}

void TcpCmdChannel::disconnect_sock() {
    if (client_sock >= 0) {  
        close(client_sock);  
        client_sock = -1;    
        std::cout << "[TCP] CMD Socket disconnected.\n";
    }
}

TcpStateChannel::TcpStateChannel(int port) : TcpBase(port) {}


bool TcpStateChannel::sendState(TcpState& stateinfo) {
    if (client_sock < 0)
        return false;

    if (send(client_sock, &stateinfo, sizeof(TcpState), 0) < 0 ) {
        close(client_sock);
        client_sock = -1;
        return false;
    }
    
    return true;
}
