#pragma once
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/Logger.hpp"
#include "image_processing_csc/ImageProcessor.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/sender.hpp"
#include "detecting_csc/edgetpu_detector.hpp"
#include "tracking_csc/sort_tracker.hpp"
#include "edgetpu.h"
/**
    TCP 소켓 연결 , cbit 및 주기 송신 담당
*/
void Task_sendState(TcpStateChannel& tcpStateChannel , BIT& bit ,Logger& logger);

/**
    운용 명령 수신 및 저장
    
*/
void Task_receiveCmd(TcpCmdChannel& tcpCmdChannel,MotorControl& motorcontrol,Logger& logger);

/** 
    모터 제어
*/
void Task_moveMotor(MotorControl& motorcontrol);


void Task_img_process(Logger& logger,CaptureUnit& capunit ,ImageProcessor& imgprocessor,tracking::ByteTracker& tracker, \
    edge::TfLiteWrapper& detector ,ThreadSafeQueue<FramePtr>& out_queue ,ThreadSafeQueue<SendPacket> &send_queue);

void Task_infer(ImageProcessor& imgprocessor ,edge::TfLiteWrapper& detector, ThreadSafeQueue<FramePtr>& in_queue);

void Task_sendImageMeta( UdpSender& sender,ThreadSafeQueue<SendPacket>& in_queue);