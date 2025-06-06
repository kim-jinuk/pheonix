
#ifndef TCPRECEIVER_HPP
#define TCPRECEIVER_HPP



#include <netinet/in.h>
#include <string>
#include <running_control_csc/globals.hpp>



class TcpBase {

protected :
    int server_sock; // listen sock
    int client_sock;
    sockaddr_in server_addr;
    sockaddr_in client_addr;
    socklen_t client_len;

public:
    TcpBase(int port);
    ~TcpBase();
    bool AcceptConnection();
};


class TcpCmdChannel : public TcpBase {
public :
    TcpCmdChannel(int port);

    bool TcpParsing(TcpCommand& cmd);
    bool sendAck(TcpCommand& cmd);
    void disconnect_sock();

};

class TcpStateChannel : public TcpBase {
public :
    TcpStateChannel(int port);

    bool sendState(TcpState& stateinfo);
};

#endif