
#ifndef TCPRECEIVER_HPP
#define TCPRECEIVER_HPP

#define TCP_MAGIC_WORD 0xA5A5

#include <netinet/in.h>
#include <string>

struct TcpCommand {
    uint8_t mode_num;
    int8_t dx, dy;
    int8_t tracking_id;
};

class TcpReceiver {
    int server_sock;
    int client_sock;
    sockaddr_in server_addr;
    sockaddr_in client_addr;
    socklen_t client_len;

public:
    TcpReceiver(int port);
    ~TcpReceiver();

    bool AcceptConnection();
    bool TcpParsing(TcpCommand& cmd);
    void TcpAngle(int8_t dx, int8_t dy);
};

#endif
