
#ifndef TCPRECEIVER_HPP
#define TCPRECEIVER_HPP



#include <netinet/in.h>
#include <string>
#include <boost/asio.hpp>
#include <iostream>
#include <cstdint>
#include "running_control_csc/globals.hpp"

#define USE_BOOST 1

#if USE_BOOST != 1
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

#else

class TcpBase {
protected:
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;

public:
    TcpBase(int port);
    virtual ~TcpBase();

    bool AcceptConnection();
};

class TcpCmdChannel : public TcpBase {
public:
    TcpCmdChannel(int port);
    bool TcpParsing(TcpCommand& cmd);
    bool sendAck(const TcpCommand& cmd);
    void disconnect_sock();
};

class TcpStateChannel : public TcpBase {
public:
    TcpStateChannel(int port);
    bool sendState(const TcpState& state);
};
#endif
#endif