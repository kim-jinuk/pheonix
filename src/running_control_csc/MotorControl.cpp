
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/pwm.hpp"
#include "running_control_csc/globals.hpp"
#include <iostream>
#include <cmath>

/* 
    SCAN
*/
SCANMotor::SCANMotor() {
    int _yaw;
    {
        std::lock_guard<std::mutex> lock(pos_mtx);
        _yaw=pos.yaw;
    }
    if (_yaw+30>=180) {
        sweep_high=180;
        sweep_low=120;
    }
    else if (_yaw-30<=0) {
        sweep_high=60;
        sweep_low=0;
    }
    else {
        sweep_high=_yaw+30;
        sweep_low=_yaw-30;
    }
    std::cout <<"sweep_high : " <<sweep_high <<" sweep_low :" << sweep_low <<std::endl;
}
SCANMotor::~SCANMotor() {
    std::cout <<"SCANMotor destroyed " << std::endl;
}

void SCANMotor::updateAngle() {

    std::cout << "SCANMotor logic"<<std::endl;
    static const int STEP = 1;

    if (pos.yaw >= sweep_high || pos.yaw <= sweep_low) {
        direction *= -1;  // 반대로 스캔
    }
    pos.yaw+= STEP*direction;
}

/*
    MANUAL
 */
ManualMotor::ManualMotor() {
    std::cout <<"ManualMotor created " << std::endl;
}

ManualMotor::~ManualMotor() {
    std::cout <<"ManualMotor destroyed " << std::endl;
}
void ManualMotor::enqueueDelta(uint8_t delta) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        delta_queue.push(delta);
}

void ManualMotor::updateAngle() {

    {
        std::lock_guard<std::mutex> lock(queue_mtx);

        if (!delta_queue.empty()) 
        {
            uint8_t cmd = delta_queue.front();
            uint8_t motor_id = (cmd >> 1 ) & 0x01;
            int8_t direction= (cmd & 0x01) ? -5: 5;

            if (motor_id == 0) {
                pos.yaw += direction;
                if (pos.yaw>=180)
                     pos.yaw=180;
                else if (pos.yaw<=0)
                    pos.yaw=0;
            }
                
            else {
                pos.pitch += direction;
                if (pos.pitch>=180)
                    pos.pitch=180;
                else if (pos.pitch<=0)
                    pos.pitch=0;
            }
            delta_queue.pop();
        }
        

    }
}

/* 
    Tracking
*/
void TrackingMotor::updateAngle() {
    std::cout << "TRACKING logic"<<std::endl;
    std::pair<int,int> targetPos=targetInfo.getXY();;

    if (targetPos.first==MISSTARGET && targetPos.second==MISSTARGET) {
        std::cout <<"Miss target"<< std::endl;
        return;
    }

    constexpr int CENTER_X = 320;  
    constexpr int CENTER_Y = 240;
    constexpr int DEADZONE = 20;  
    constexpr int ZONE1 = 50;     
    constexpr int ZONE2 = 100;    

    int dx = targetPos.first - CENTER_X;
    int dy = targetPos.second - CENTER_Y;

    if (std::abs(dx) <= DEADZONE) {
        std::cout << "Yaw: Deadzone, no move" << std::endl;
    } else if (std::abs(dx) <= ZONE1) {
        std::cout << "Yaw: small adjust" << std::endl;
        pos.yaw+=static_cast<int>(dx * 0.2);
    } else {
        std::cout << "Yaw: strong adjust" << std::endl;
        pos.yaw+=static_cast<int>(dx * 0.4);
    }

    // pitch 방향 제어
    if (std::abs(dy) <= DEADZONE) {
        std::cout << "Pitch: Deadzone, no move" << std::endl;
    } else if (std::abs(dy) <= ZONE1) {
        std::cout << "Pitch: small adjust" << std::endl;
        pos.pitch+=static_cast<int>(dy * 0.1);
    } else {
        std::cout << "Pitch: strong adjust" << std::endl;
        pos.pitch+=static_cast<int>(dy * 0.2);
    }

    
    if (pos.yaw>=180)
        pos.yaw=180;
    else if (pos.yaw<=0)
        pos.yaw=0;

    if (pos.pitch>=180)
        pos.pitch=180;
    else if (pos.pitch<=0)
        pos.pitch=0;
}

/*
    MotorControl
*/
void MotorControl::init_pos() {
    std::cout << "init motor"<<std::endl;
    pwm_init();
}

void MotorControl::move() {
   // std::cout << "move motor"<<std::endl;
  //  std::cout << "Yaw:" <<pos.yaw << " Pitch:" << pos.pitch <<std::endl;
    pwm(pos.yaw,pos.pitch);
}

void MotorControl::setStrategy(uint8_t new_mode) {
    

    if (current_type == new_mode) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(strategy_mtx);
        switch (new_mode) {
            case static_cast<uint8_t>(Mode::SCAN):
                strategy = std::make_shared<SCANMotor>();
                break;
            case static_cast<uint8_t>(Mode::MANUAL):
                strategy = std::make_shared<ManualMotor>();
                break;
            case static_cast<uint8_t>(Mode::TRACKING):
                strategy = std::make_shared<TrackingMotor>();
                break;
            default:
                strategy = nullptr;
                break;
        }
    }

    current_type = new_mode;
}

void MotorControl::runStrategy() {

    std::shared_ptr<IMotorStrategy> local;
    {
        std::lock_guard<std::mutex> lock(strategy_mtx);
        local = strategy;
    }

    {
        std::lock_guard<std::mutex> lock(pos_mtx);
        if (local) local->updateAngle();
        move();
        
    }
}
void MotorControl::enqueueDeltaIfManual(uint8_t delta) {

    std::lock_guard<std::mutex> lock(strategy_mtx);
        if (current_type == static_cast<int>(Mode::MANUAL)) {
            if (auto manual = std::dynamic_pointer_cast<ManualMotor>(strategy)) {
                manual->enqueueDelta(delta);
            }
        }
    
}