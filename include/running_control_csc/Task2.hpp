
#include "running_control_csc/Task.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/sender.hpp"
#include "running_control_csc/globals.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/Logger.hpp"
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/Logger.hpp"
#include "running_control_csc/globals.hpp"
#include "image_processing_csc/ImageProcessor.hpp"
#include "detecting_csc/edgetpu_detector.hpp"
#include "tracking_csc/sort_tracker.hpp"
#include "edgetpu.h"
#include "opencv2/opencv.hpp"
#include <vector>
#include <chrono>
#include <atomic>
#include <iostream>
#include <thread>
/**
    주기 통신, Tcpsate 전송
    Checking -> IDLE , Checking ->IDLE , RUNNING -> CHECKING
*/
void Task_sendState(TcpStateChannel& tcpStateChannel , BIT& bit ,Logger& logger );
/**
    명령 송신
    IDLE -> RUNNING , RUNNING -> IDLE
*/
void Task_receiveCmd(TcpCmdChannel& tcpCmdChannel , MotorControl& motorcontrol,Logger& logger);
      
void Task_img_process(Logger& logger, CaptureUnit& capunit ,ImageProcessor& imgprocessor,tracking::ByteTracker& tracker, \
    edge::TfLiteWrapper& detector ,MotorControl& motorcontrol,ThreadSafeQueue<FramePtr>& out_queue ,ThreadSafeQueue<SendPacket> &send_queue);


void Task_infer(ImageProcessor& imgprocessor ,edge::TfLiteWrapper& detector, ThreadSafeQueue<FramePtr>& in_queue);
     

void Task_sendImageMeta( UdpSender& sender,ThreadSafeQueue<SendPacket>& in_queue);