#pragma once
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/Task.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/Logger.hpp"
#include "image_processing_csc/ImageProcessor.hpp"

/**
    TCP 소켓 연결 , cbit 및 주기 송신 담당
*/
void Task_sendState(TcpStateChannel& tcpStateChannel , BIT& bit ,Logger& logger);

/**
    운용 명령 수신 및 저장
    
*/
void Task_receiveCmd(TcpCmdChannel& tcpCmdChannel,MotorControl& motorcontrol,Logger& logger);

/**
    패킷 생성, 로깅, 패킷 송신
*/
void Task_sendData(Logger& logger);


/** 
    모터 제어
*/
void Task_moveMotor(MotorControl& motorcontrol);

