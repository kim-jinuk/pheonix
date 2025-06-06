
#include "running_control_csc/Task.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/sender.hpp"
#include "running_control_csc/globals.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/Logger.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
/**
    주기 통신, Tcpsate 전송
    Checking -> IDLE , Checking ->IDLE
*/
void Task_sendState(TcpStateChannel& tcpStateChannel , BIT& bit ,Logger& logger ) {
    

    while (true) {
        
        if (!tcpStateChannel.AcceptConnection()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        std::cout <<" msbmsbmsbmsb" << std::endl;
        //std::cout <<"..." <<std::endl;
        sysInfo.TCP_state_connected.store(true);
        while (sysInfo.TCP_state_connected.load()==true) {
            bit.cbit();
            TcpState tcpstate;
            // 0 CHECKING , 1 IDLE , 2 RUNNGING
            tcpstate.state_num=static_cast<uint8_t>(sysInfo.current_state.load());
            tcpstate.tpu=sysInfo.TPU_state;
            tcpstate.cam=sysInfo.CAM_state;
            tcpstate.sdcard=sysInfo.logging_enabled;
            
            // change state depending on device
            if (!(sysInfo.TPU_state && sysInfo.CAM_state)) {
                sysInfo.current_state.store(State::CHECKING);
                tcpstate.state_num=static_cast<int>(State::CHECKING);
            }
            // device ok
            else {
                if (tcpstate.state_num==static_cast<int>(State::CHECKING)) {
                    sysInfo.current_state.store(State::IDLE);
                    tcpstate.state_num=static_cast<int>(State::IDLE);
                }
            }
            tcpstate.mode_num=static_cast<uint8_t>(sysInfo.current_mode.load());

            if (tcpstate.state_num!=static_cast<int>(State::CHECKING))
            {
                std::lock_guard<std::mutex> lock(pos_mtx);
                tcpstate.Nx=pos.yaw;
                tcpstate.Ny=pos.pitch;
            }

            std::cout << "statd :"<< tcpstate.state_num<< " mode :" <<tcpstate.mode_num <<std::endl;
            if (!tcpStateChannel.sendState(tcpstate)) {
                sysInfo.TCP_state_connected.store(false); 
                std::cout <<"send state failed" <<std::endl;
                sysInfo.current_state.store(State::CHECKING);
                break;
            }
            

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
          //  std::cout <<"send loop doing..." <<std::endl;
        }

    }

}

/**
    명령 송신
    IDLE -> RUNNING , RUNNING -> IDLE
*/
void Task_receiveCmd(TcpCmdChannel& tcpCmdChannel , MotorControl& motorcontrol,Logger& logger) {

    while (true) {
        // 주기적으로 확인하도록 하자...
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait_for(lock,std::chrono::seconds(1), [] \
            { return sysInfo.current_state.load() \
                ==State::IDLE; }); 
        }
        std::cout <<" wake up receive thread" << std::endl;
        if (!tcpCmdChannel.AcceptConnection()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        sysInfo.TCP_cmd_connected.store(true);

        while (sysInfo.TCP_cmd_connected.load()==true) {
            TcpCommand cmd;
            std::cout << "... : " << std::endl;
            // receive
            if (!tcpCmdChannel.TcpParsing(cmd)) {
                sysInfo.TCP_cmd_connected.store(false); 
                sysInfo.current_state.store(State::IDLE);
                break;
            }
            std::cout << "flag : " << static_cast<int>(cmd.cmd_flag) \
            << " cmd : " << static_cast<int>(cmd.cmd) << std::endl;
            switch(cmd.cmd_flag) {
                case Mode_num :
                    sysInfo.current_mode.store(static_cast<Mode>(cmd.cmd));
                    motorcontrol.setStrategy(cmd.cmd);
                    break;
                case Cam_num :
                    std::cout<< "change EO/IR"<<std::endl;
                    cam_opt.eo_ir.store(cmd.cmd);
                    break;
                case Prep_opt :
                    std::cout<< "set Prep_opt"<<std::endl;
                    cam_opt.fromCmd(cmd.cmd);
                    break;
                case move_motor :
                    if (sysInfo.current_mode.load() == Mode::MANUAL) {
                        motorcontrol.enqueueDeltaIfManual(cmd.cmd);
                    }
                    break;
                case track :
                    targetInfo.id.store(cmd.cmd);
                    sysInfo.current_mode.store(static_cast<Mode>(cmd.cmd));
                    motorcontrol.setStrategy(cmd.cmd);
                    break;
                default :
                    break;
            }
            // ack
            if (!tcpCmdChannel.sendAck(cmd)) {
                sysInfo.TCP_cmd_connected.store(false); 
                sysInfo.current_state.store(State::IDLE);
                break;
            }

        }

    }

}

void Task_sendData(UdpSender& sender ,Logger& logger) {


    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }
        std::cout << "Task_sendData thread wake up" <<std::endl;
        while (sysInfo.current_state.load() == State::RUNNING) {
             /*
             TODO
             */
             /*
             1. 이미지 , 추론 데이터 , 모터 각 읽기
             2. 패킷 생성 및 송신
             3. 로깅
             */

             // 1


           
            
            // 2

            // auto packets = sender.BuildUdpPackets({}, _pos.yaw, _pos.pitch); // object 정보 들어갈 것임
            // sender.send(packets);

            // 3
          //  logger.logOperation( Mode_str[static_cast<int>(sysInfo.current_mode.load())] ,"meta" ,_pos.yaw, _pos.pitch);

            

            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

}



void Task_moveMotor(MotorControl& motorcontrol) {
    
    std::cout << "motor thread is created" <<std::endl;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }
        std::cout << "motor thread wake up" <<std::endl;
        motorcontrol.init_pos();
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            /*
            전략이 실행되는 곳
            */
            motorcontrol.runStrategy();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }


}

void Task_ImageProcessing() {
    
    std::cout << "ImageProcessing thread is created" <<std::endl;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }

        std::cout << "ImageProcessingor thread wake up" <<std::endl;
        
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            /*
                1. capture
                2. preprocessing
                3. infer
                4. overlay
                5. push 
            */
        }
    }

}
