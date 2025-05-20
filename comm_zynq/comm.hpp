#ifndef COMMUNICATION_HPP
#define COMMUNICATION_HPP

#include <string>
#include <map>

extern std::string UDP_IP;
extern int UDP_PORT;
extern int TCP_PORT;
extern int ANGLE_TCP_PORT;

std::map<std::string, std::string> load_config(const std::string& filename);

// 영상 수신
void udp_sender(const std::string& ip, int port);

// 모터 제어, 모드 전환 명령 송신
void tcp_receiver(int port);

// 현재 모터 값 수신
void angle_sender(const std::string& ip, int port);

#endif // COMM_HPP 