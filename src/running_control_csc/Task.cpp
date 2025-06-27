
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
    Checking -> IDLE , Checking ->IDLE , RUNNING -> CHECKING
*/
void Task_sendState(TcpStateChannel& tcpStateChannel , BIT& bit ,Logger& logger ) {
    

    while (true) {
        
        if (!tcpStateChannel.AcceptConnection()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        sysInfo.TCP_state_connected.store(true);
        while (sysInfo.TCP_state_connected.load()==true) {
            bit.cbit();
            TcpState tcpstate;
            // 0 CHECKING , 1 IDLE , 2 RUNNGING
            tcpstate.state_num=static_cast<uint8_t>(sysInfo.current_state.load());
            tcpstate.tpu=sysInfo.TPU_state;
            tcpstate.cam=sysInfo.CAM_state;
            tcpstate.sdcard=sysInfo.logging_enabled;
            tcpstate.cpu_temp=sysInfo.cpu_temp;
            
            // change state depending on device
            if (!(sysInfo.TPU_state && sysInfo.CAM_state)) {
                sysInfo.current_state.store(State::CHECKING);
                tcpstate.state_num=static_cast<int>(State::CHECKING);
            }
            // device ok
            else {
                if (tcpstate.state_num==static_cast<uint8_t>(State::CHECKING)) {
                    sysInfo.current_state.store(State::IDLE);
                    tcpstate.state_num=static_cast<uint8_t>(State::IDLE);
                }
            }
            tcpstate.mode_num=static_cast<uint8_t>(sysInfo.current_mode.load());

            if (tcpstate.state_num!=static_cast<int>(State::CHECKING))
            {
                std::lock_guard<std::mutex> lock(pos_mtx);
                tcpstate.Nx=pos.yaw;
                tcpstate.Ny=pos.pitch;
            }

           // std::cout << "state :"<< tcpstate.state_num<< " mode :" <<tcpstate.mode_num <<std::endl;
            if (!tcpStateChannel.sendState(tcpstate)) {
                sysInfo.TCP_state_connected.store(false); 
             //   std::cout <<"send state failed" <<std::endl;
                sysInfo.current_state.store(State::CHECKING);
                break;
            }
            
            logger.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
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
        
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait_for(lock,std::chrono::seconds(1), [] \
            { return sysInfo.current_state.load() \
                ==State::IDLE; }); 
        }
    //    std::cout <<" wake up CMD thread" << std::endl;
        if (!tcpCmdChannel.AcceptConnection()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        sysInfo.TCP_cmd_connected.store(true);
        // IDLE 상태에서 tcp 요청받으면 IDLE -> RUNNING
        {
            std::lock_guard<std::mutex> lock(statesync.mtx);
            sysInfo.current_state.store(State::RUNNING);
            sysInfo.current_mode.store(Mode::MANUAL);
           // logger.logStateChange(State_str[state],State_str[static_cast<int>(State::RUNNING)]);
        }
        statesync.cv.notify_all();
        
        motorcontrol.setStrategy(static_cast<uint8_t>(Mode::MANUAL));
        {
            std::lock_guard<std::mutex> lock(pos_mtx);
            pos.yaw=90;
            pos.pitch=90;
        }
     //   std::cout << "set MANUAL motor" << std::endl;
        while (sysInfo.current_state.load()==State::RUNNING) {
            TcpCommand cmd;
      //      std::cout << "CMD thread Wait CMD... " << std::endl;
            
            if (!tcpCmdChannel.TcpParsing(cmd)) {
                sysInfo.TCP_cmd_connected.store(false); 
                sysInfo.current_state.store(State::IDLE);
                break;
            }
            // std::cout << "flag : " << static_cast<int>(cmd.cmd_flag) \
            // << " cmd : " << static_cast<int>(cmd.cmd) << std::endl;
            switch(cmd.cmd_flag) {
                case Mode_num :
         //           std::cout<< "change mode"<<std::endl;
                    sysInfo.current_mode.store(static_cast<Mode>(cmd.cmd));
                    motorcontrol.setStrategy(cmd.cmd);
                    break;
                case Cam_num :
         //           std::cout<< "change EO/IR"<<std::endl;
                    cam_opt.eo_ir.store(cmd.cmd);

                    break;
                case Prep_opt : {
           //         std::cout<< "set Prep_opt"<<std::endl;
                    cam_opt.fromCmd(cmd.cmd);
                    int a=cam_opt.enhance_edges.load();
                    int b=cam_opt.enhance_contrast.load();
             //       std::cout<< "edges : "<< a << " contrast : "<< b << std::endl;
                    break;
                }
                case move_motor :
                    if (sysInfo.current_mode.load() == Mode::MANUAL) {
                        motorcontrol.enqueueDeltaIfManual(cmd.cmd);
                    }
                    break;
                case track :
                    targetInfo.id.store(cmd.cmd);
                    sysInfo.current_mode.store(Mode::TRACKING);
                    motorcontrol.setStrategy(static_cast<uint8_t>(Mode::TRACKING));
                    break;
                case InitMotor :
                    if (sysInfo.current_mode.load()==(Mode::MANUAL)) {
                        std::lock_guard<std::mutex> lock(pos_mtx);
                        pos.yaw=90;
                        pos.pitch=90;
                    }
                default :
                    break;
            }
            logger.logCmd(cmd.cmd_flag,cmd.cmd);
            // ack
            if (!tcpCmdChannel.sendAck(cmd)) {
                sysInfo.TCP_cmd_connected.store(false); 
                sysInfo.current_state.store(State::IDLE);
                break;
            }

        }
        // 장치 이상으로 (다른 스레드가 RUNNING에서 다른 상태로 보내면) 기존 소켓 닫기
        tcpCmdChannel.disconnect_sock();
        motorcontrol.setStrategy(static_cast<uint8_t>(Mode::DEFAULT));
  //      std::cout << "set DEFAULt motor" << std::endl;
       // std::cout << "TCP cmd channel disconnected, thread sleep 0.3s..." << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

}


void Task_moveMotor(MotorControl& motorcontrol) {
    
  //  std::cout << "motor thread is created" <<std::endl;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }
     //   std::cout << "motor thread wake up" <<std::endl;
        motorcontrol.init_pos();
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            motorcontrol.runStrategy();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }


}

