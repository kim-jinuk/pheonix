
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

#include <pthread.h>
#include <sched.h>
#include <iostream>
#include <unistd.h>

void SetRealTimePriority() {
    sched_param sch_params;
    sch_params.sched_priority = 80;  // 1~99 (실시간)

    pthread_t this_thread = pthread_self();
    if (pthread_setschedparam(this_thread, SCHED_FIFO, &sch_params)) {
        std::cerr << "[WARN] Failed to set real-time priority. Are you root?" << std::endl;
    } else {
        std::cout << "[OK] Real-time priority set!" << std::endl;
    }
}

void PinToCore(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);  // core_id=0 → 첫 번째 코어

    pthread_t this_thread = pthread_self();
    if (pthread_setaffinity_np(this_thread, sizeof(cpu_set_t), &cpuset)) {
        std::cerr << "[WARN] Failed to pin thread to core." << std::endl;
    } else {
        std::cout << "[OK] Thread pinned to CPU core " << core_id << std::endl;
    }
}

/**
    주기 통신, Tcpsate 전송
    Checking -> IDLE , Checking ->IDLE , RUNNING -> CHECKING
*/
void Task_sendState(TcpStateChannel& tcpStateChannel , BIT& bit ,Logger& logger ) {
    
    pthread_setname_np(pthread_self(), "sendstate");
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

            std::cout << "state :"<< tcpstate.state_num<< " mode :" <<tcpstate.mode_num <<std::endl;
            if (!tcpStateChannel.sendState(tcpstate)) {
                sysInfo.TCP_state_connected.store(false); 
                std::cout <<"send state failed" <<std::endl;
                sysInfo.current_state.store(State::CHECKING);
                break;
            }
            

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
          //  std::cout <<"send loop doing..." <<std::endl;
        }

    }

}

/**
    명령 송신
    IDLE -> RUNNING , RUNNING -> IDLE
*/
void Task_receiveCmd(TcpCmdChannel& tcpCmdChannel , MotorControl& motorcontrol,Logger& logger) {
    pthread_setname_np(pthread_self(), "cmdreceive");
    while (true) {
        // 주기적으로 확인하도록 하자...
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait_for(lock,std::chrono::seconds(1), [] \
            { return sysInfo.current_state.load() \
                ==State::IDLE; }); 
        }
        std::cout <<" wake up CMD thread" << std::endl;
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
        while (sysInfo.current_state.load()==State::RUNNING) {
            TcpCommand cmd;
            std::cout << "CMD thread Wait CMD... " << std::endl;
            
            if (!tcpCmdChannel.TcpParsing(cmd)) {
                sysInfo.TCP_cmd_connected.store(false); 
                sysInfo.current_state.store(State::IDLE);
                break;
            }
            // std::cout << "flag : " << static_cast<int>(cmd.cmd_flag) \
            // << " cmd : " << static_cast<int>(cmd.cmd) << std::endl;
            switch(cmd.cmd_flag) {
                case Mode_num :
                    std::cout<< "change mode"<<std::endl;
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
                    std::cout<< "move motor"<<std::endl;
                    if (sysInfo.current_mode.load() == Mode::MANUAL) {
                        motorcontrol.enqueueDeltaIfManual(cmd.cmd);
                    }
                    break;
                case track :
                    std::cout<< "do tracking"<<std::endl;
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
        // 장치 이상으로 (다른 스레드가 RUNNING에서 다른 상태로 보내면) 기존 소켓 닫기
        tcpCmdChannel.disconnect_sock();
        std::cout << "TCP cmd channel disconnected, thread sleep 0.3s..." << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

}
void Task_sendData(UdpSender& sender) {
    SetRealTimePriority();
    PinToCore(0);
    pthread_setname_np(pthread_self(), "udpsend");
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] {
                return sysInfo.current_state.load() == State::RUNNING;
            });
        }

        std::cout << "Task_sendData thread wake up" << std::endl;

        while (sysInfo.current_state.load() == State::RUNNING) {

            if (!sysInfo.TCP_cmd_connected.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            auto t0 = std::chrono::high_resolution_clock::now();
            // TCP 연결되어 있고 RUNNING 상태일 때만 데이터 전송
            sender.CaptureAndStorePayload();
            
            // std::cout << "Task_sendData capture data" << std::endl;
            auto t1 = std::chrono::high_resolution_clock::now();
            uint8_t nx = 0, ny = 0;
            // {
            //     std::lock_guard<std::mutex> lock(angle_mutex);
            //     // 필요한 경우 nx, ny 값 갱신
            // }

            auto packets = sender.BuildUdpPackets({}, nx, ny);
            sender.UdpSend(packets);
            auto t2 = std::chrono::high_resolution_clock::now();
            // std::cout << "Task_sendData send data" << std::endl;
            std::cout << "[Timing] Capture: "
          << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
          << " ms, Send: "
          << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
          << " ms, Total: "
          << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t0).count()
          << " ms\n";
           // std::this_thread::sleep_for(std::chrono::milliseconds(33));
            // 패킷 송신 평균 70ms 
        }

        // std::cout << "[UDP] Waiting for TCP reconnection...\n";

    }
}


void Task_moveMotor(MotorControl& motorcontrol) {
    pthread_setname_np(pthread_self(), "motor");
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
