#ifndef RECEIVER_BOOST_HPP
#define RECEIVER_BOOST_HPP

#include <boost/asio.hpp>
#include <iostream>
#include <cstdint>

#include <running_control_csc/globals.hpp>


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
