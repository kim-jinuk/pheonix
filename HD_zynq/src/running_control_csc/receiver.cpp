#include "running_control_csc/receiver.hpp"

#include <arpa/inet.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <iostream>
#include <memory>
#include <cstring>



using boost::asio::ip::tcp;

TcpBase::TcpBase(int port)
    : acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "[BOOST TCP] Server listening on port " << port << "\n";
}

TcpBase::~TcpBase() {
    if (socket_ && socket_->is_open()) {
        socket_->close();
    }
}

bool TcpBase::AcceptConnection() {
    try {
        socket_ = std::make_unique<tcp::socket>(io_context_);
        acceptor_.accept(*socket_);
        std::cout << "[BOOST TCP] Client connected\n";
        return true;
    } catch (std::exception& e) {
        std::cerr << "[ERR] Accept failed: " << e.what() << "\n";
        return false;
    }
}

// ====================== TcpCmdChannel ======================

TcpCmdChannel::TcpCmdChannel(int port) : TcpBase(port) {}

bool TcpCmdChannel::TcpParsing(TcpCommand& cmd) {
    try {
        boost::asio::read(*socket_, boost::asio::buffer(&cmd, sizeof(TcpCommand)));
        std::cout << std::hex << cmd.magic_word << std::endl;

        if (cmd.magic_word != TCP_MAGIC_WORD) {
            std::cerr << "[BOOST TCP] Invalid magic word\n";
            return true;  // 연결 유지, 패킷만 skip
        }

        return true;
    } catch (...) {
        std::cerr << "[BOOST TCP] Client disconnected or error\n";
        socket_.reset();
        return false;
    }
}

bool TcpCmdChannel::sendAck(const TcpCommand& cmd) {
    if (!socket_ || !socket_->is_open()) return false;

    try {
        boost::asio::write(*socket_, boost::asio::buffer(&cmd, sizeof(TcpCommand)));
        return true;
    } catch (...) {
        socket_.reset();
        return false;
    }
}

void TcpCmdChannel::disconnect_sock() {
    if (socket_ && socket_->is_open()) {
        socket_->close();
        std::cout << "[BOOST TCP] CMD socket disconnected\n";
    }
}

// ====================== TcpStateChannel ======================

TcpStateChannel::TcpStateChannel(int port) : TcpBase(port) {}

bool TcpStateChannel::sendState(const TcpState& state) {
    if (!socket_ || !socket_->is_open()) return false;

    try {
        boost::asio::write(*socket_, boost::asio::buffer(&state, sizeof(TcpState)));
        return true;
    } catch (...) {
        socket_.reset();
        return false;
    }
}
