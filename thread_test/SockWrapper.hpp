
#ifndef SOCKWRAPPER_HPP
#define SOCKWRAPPER_HPP

#include <string>
#include <netinet/in.h>

class SockWrapper {
public:
    static int create_udp_socket();
    static int create_tcp_socket();
    static void close_socket(int sock);
    static sockaddr_in make_addr(const std::string& ip, int port);
};

#endif
