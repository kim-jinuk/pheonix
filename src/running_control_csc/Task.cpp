
#include "running_control_csc/Task.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/globals.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/globals.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
void Task_sendState(TcpReceiver& tcpchannel , BIT& bit ) {
    

    while (true) {
        
        if (!tcpchannel.AcceptConnection()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        sysInfo.TCP_connect.store(true);
        while (sysInfo.TCP_connect.load()==true) {
            
            bit.cbit(); // 상태 업데이트

            int state, mode;
            bool device_ok = sysInfo.CAM_state && sysInfo.TPU_state;
            state=static_cast<int>(sysInfo.current_state.load()); // 0 CHECKING , 1 IDLE , 2 RUNNGING
            if (!device_ok) 
                sysInfo.current_state.store(State::CHECKING);
            else {
                if (state==static_cast<int>(State::CHECKING)) {
                    sysInfo.current_state.store(State::IDLE);
                }
            }

            mode=static_cast<int>(sysInfo.current_mode.load());
            
            

            if (/*!send*/ !tcpchannel.sendState(sysInfo.TPU_state, sysInfo.CAM_state, state,mode)) {
                sysInfo.TCP_connect.store(false); 

                sysInfo.current_state.store(State::CHECKING);
            }
            /*
            delay
            */
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

    }

}

void Task_receiveCmd(TcpReceiver& tcpchannel , MotorControl& motorcontrol) {

    while (true) {
        
        while (sysInfo.TCP_connect.load()==true) {
            TcpCommand cmd;
            
            if (/*recv 메서드*/tcpchannel.TcpParsing(cmd)) {
                std::cout << "parsing" <<std::endl;
                int state=static_cast<int>(sysInfo.current_state.load());


                if (state==static_cast<int>(State::CHECKING)) {
                    continue;
                }
                else if (state==static_cast<int>(State::IDLE)) {
                    {
                     std::lock_guard<std::mutex> lock(statesync.mtx);
                     sysInfo.current_state.store(State::RUNNING);
                     sysInfo.current_mode.store(Mode::MANUAL);
                    }
                    statesync.cv.notify_all();
                    continue;
                }

                else {
                    /*
                    TODO : 로직 넣기
                    */
                    if (cmd.mode_num<3){
                         sysInfo.current_mode.store(static_cast<Mode>(cmd.mode_num));
                         motorcontrol.setStrategy(cmd.mode_num);
                    }
                    if (sysInfo.current_mode.load() == Mode::MANUAL) {
                        motorcontrol.enqueueDeltaIfManual(cmd.dx, cmd.dy);
                    }

                }
                
            }

            else 
            {
                sysInfo.TCP_connect.store(false);
                sysInfo.current_state.store(State::CHECKING);
            }
        }

    }

}

void Task_sendData() {


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


}

void Task_infer() {


}