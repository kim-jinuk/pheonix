

#include "running_control_csc/BIT.hpp"
#include "running_control_csc/globals.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/CfgLoader.hpp"
#include "running_control_csc/Task.hpp"
#include "running_control_csc/ImgTaskv1.hpp"
#include "running_control_csc/ImgTaskv2.hpp"
#include "running_control_csc/Logger.hpp"
#include "running_control_csc/sender.hpp"
#include "image_processing_csc/ImageProcessor.hpp"
#include "detecting_csc/edgetpu_detector.hpp"
#include "tracking_csc/sort_tracker.hpp"
#include <signal.h>
#include <thread>
#include <iostream>
#include <atomic>
#include <unistd.h>
#define IMG_VERSION 2

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
    
    std::string model_path = cfg.get("MODEL");
    std::string label_path = cfg.get("LABEL");
    float threshold = std::stof(cfg.get("THRESHOLD"));

    bool use_edgetpu = (cfg.get("USE_EDGETPU") == "1");

    std::shared_ptr<edgetpu::EdgeTpuContext> context;
    if (use_edgetpu) {
        context = edgetpu::EdgeTpuManager::GetSingleton()->OpenDevice();
    }

    /* detector*/
    edge::TfLiteWrapper detector(
        model_path,
        label_path,
        threshold,
        context,
        use_edgetpu
    );
    /* tracker*/
    tracking::SortTracker tracker(0.3f);
    /* Image processor*/
    ImageProcessor imgprocessor;
    /* BIT */
    BIT bit;
    /* TCPcmd */
    TcpCmdChannel tcpCmdChannel(tcp_cmd_port);
    /* TCPstate*/
    TcpStateChannel tcpStateChannel(tcp_state_port);
    /* UDP */
    UdpSender sender(udp_ip, udp_port);
    /* Motor*/
    MotorControl motorcontrol;
    /* capunit*/
    CaptureUnit capunit;
    Logger logger("./logs");
    
    bit.pbit();
   
    std::thread sendStateThread(Task_sendState, std::ref(tcpStateChannel), std::ref(bit), std::ref(logger)); 
    std::thread receiveCmdThread(Task_receiveCmd, std::ref(tcpCmdChannel),std::ref(motorcontrol),std::ref(logger)); 
    std::thread moveMotorThread(Task_moveMotor,std::ref(motorcontrol)); 

#if IMG_VERSION == 1
    std::thread ImageProcessingThread(Task_ImageProcessing, std::ref(capunit),std::ref(imgprocessor),\
                                    std::ref(detector),std::ref(tracker),std::ref(sender));
#elif IMG_VERSION == 2
    std::thread image_processThread(Task_img_process, std::ref(capunit),std::ref(imgprocessor),std::ref(sender),std::ref(tracker),  std::ref(detector),std::ref(enhance_to_infer));
    std::thread inferThread(Task_infer, std::ref(detector), std::ref(enhance_to_infer));
#endif



    while (true) {
    std::this_thread::sleep_for(std::chrono::hours(24));
    }

}