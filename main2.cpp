

#include "running_control_csc/BIT.hpp"
#include "running_control_csc/globals.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/CfgLoader.hpp"
#include "running_control_csc/Task2.hpp"
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
#define IMG_VERSION 10

using namespace std;

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
    
    
    /* tracker*/
#if TRACKING_VERSION == 1
    tracking::SortTracker tracker(0.3f);
   // tracking::SortTracker tracker(0.2f);
#else
    tracking::ByteTracker tracker;
#endif
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
    

    std::thread sendStateThread(Task_sendState, std::ref(tcpStateChannel), std::ref(bit), std::ref(logger)); 
    bit.pbit();
   

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
    std::thread receiveCmdThread(Task_receiveCmd, std::ref(tcpCmdChannel),std::ref(motorcontrol),std::ref(logger)); 
    std::thread image_processThread(Task_img_process, std::ref(logger) ,std::ref(capunit),std::ref(imgprocessor), std::ref(tracker), \
                                     std::ref(detector),std::ref(motorcontrol),std::ref(enhance_to_infer),std::ref(send_queue));
    std::thread inferThread(Task_infer,std::ref(imgprocessor), std::ref(detector), std::ref(enhance_to_infer));
    std::thread sendImgThread(Task_sendImageMeta, std::ref(sender), std::ref(send_queue));

    while (true) {
    std::this_thread::sleep_for(std::chrono::hours(24));
    }

}