

#include "running_control_csc/BIT.hpp"
#include "running_control_csc/globals.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/sender.hpp"  
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/CfgLoader.hpp"
#include "running_control_csc/Task.hpp"
#include "running_control_csc/Logger.hpp"

#include <signal.h>
#include <thread>
#include <iostream>
#include <atomic>
#include <unistd.h>
using namespace std;
std::ostream& operator<<(std::ostream& os, State s) {
    switch (s) {
        case State::CHECKING: return os << "CHECKING";
        case State::IDLE:     return os << "IDLE";
        case State::RUNNING:  return os << "RUNNING";
        default:              return os << "UNKNOWN";
    }
}

int main() {
    // 소켓 닫혔는데 send할 경우 방지용
    signal(SIGPIPE, SIG_IGN);

    // IP 할당용
    CfgLoader cfg;
    if (!cfg.load("config.cfg")) {
        std::cerr << "[ERR] Failed to load config.cfg\n";
        return 1;
    }

    std::string udp_ip = cfg.get("UDP_IP");
    int udp_port = std::stoi(cfg.get("UDP_PORT"));
    int tcp_cmd_port = std::stoi(cfg.get("TCP_CMD_PORT"));
    int tcp_state_port = std::stoi(cfg.get("TCP_STATE_PORT"));
    
    // 객체 생성 - CFGLoader 때문에 main쪽에서 객체 생성함
    BIT bit;
    TcpCmdChannel tcpCmdChannel(tcp_cmd_port); //listen socket 생성
    TcpStateChannel tcpStateChannel(tcp_state_port);
    MotorControl motorcontrol;
    Logger logger("./logs");
    UdpSender sender(udp_ip, udp_port);
    std::cout << sizeof(TcpCommand) <<std::endl;
    /**
        초기 장치 점검 수행행
    */
    bit.pbit();
    /**
        thread 생성
    */
    std::thread sendStateThread(Task_sendState, std::ref(tcpStateChannel), std::ref(bit), std::ref(logger)); 
    std::thread receiveCmdThread(Task_receiveCmd, std::ref(tcpCmdChannel),std::ref(motorcontrol),std::ref(logger)); 
    std::thread moveMotorThread(Task_moveMotor,std::ref(motorcontrol)); 
    
    std::thread sendDataThread;
    bool threadRunning = false;

    while (true) {
        bool connected = sysInfo.TCP_cmd_connected.load();

        if (connected && !threadRunning) {
            sendDataThread = std::thread([&sender]() {
                                        Task_sendData(sender);
                                    });
            threadRunning = true;
        } else if (!connected && threadRunning) {
            if (sendDataThread.joinable()) {
                // 이 시점에서는 Task_sendData 내부 루프가 종료되어야 함
                sendDataThread.join();  // 안전하게 종료 기다림
            }
            threadRunning = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}