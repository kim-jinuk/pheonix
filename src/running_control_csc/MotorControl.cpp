
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/pwm.hpp"
#include "running_control_csc/globals.hpp"
#include <iostream>
#include <cmath>
#include <unistd.h>
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

   // std::cout << "SCANMotor logic"<<std::endl;
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
  //  std::cout << "ManualMotor logic"<<std::endl;
    {
        std::lock_guard<std::mutex> lock(queue_mtx);
        int yaw=pos.yaw;
        int pitch=pos.pitch;
        if (!delta_queue.empty()) 
        {
            uint8_t cmd = delta_queue.front();
            uint8_t motor_id = (cmd >> 1 ) & 0x01;
            int direction= (cmd & 0x01) ? -5: 5;
            
            if (motor_id == 0) 
            {
                
                yaw += direction;
                if (yaw>=180)
                     yaw=180;
                else if (yaw<=0)
                    yaw=0;
            }
                
            else 
            {
                
                pitch += direction;
                if (pitch>=180)
                    pitch=180;
                else if (pitch<=0)
                    pitch=0;
            }
            delta_queue.pop();
        }
        pos.yaw=yaw;
        pos.pitch=pitch;
       // std::cout << "yaw:" << pos.yaw << " pitch : " <<pos.pitch<<std::endl;
    }
    
}

/* 
    Tracking
*/
void TrackingMotor::updateAngle() {
   // std::cout << "TRACKING logic"<<std::endl;
    std::pair<int16_t,int16_t> targetPos=targetInfo.getXY();;
    //std::cout << "target x:" << targetPos.first << "target y:" << targetPos.second <<std::endl;
    if (targetPos.first==MISSTARGET && targetPos.second==MISSTARGET) {
        //std::cout <<"Miss target"<< std::endl;
        return;
    }

    constexpr int CENTER_X = 320;  
    constexpr int CENTER_Y = 240;
    constexpr int DEADZONE = 100;  
    constexpr int ZONE1 = 50;     
    constexpr int ZONE2 = 100;    
    int yaw=pos.yaw;
    int pitch=pos.pitch;
    int dx =static_cast<int>(targetPos.first) - CENTER_X;
    int dy =static_cast<int>(targetPos.second) - CENTER_Y;
   // std::cout << "dx: " <<dx << " dy :"<< dy<<std::endl; 

    if (std::abs(dx) <= DEADZONE) {
    //    std::cout << "Yaw: Deadzone, no move" << std::endl;
    } else if (std::abs(dx) <= ZONE1) {
     //   std::cout << "Yaw: small adjust" << std::endl;
        yaw+=-dx/std::abs(dx);
    } else {
    //    std::cout << "Yaw: strong adjust" << std::endl;
        yaw+=-2*dx/std::abs(dx);
    }

    // pitch 방향 제어
    if (std::abs(dy) <= DEADZONE) {
        std::cout << "Pitch: Deadzone, no move" << std::endl;
    } 
    else {
        std::cout << "Pitch: small adjust" << std::endl;
        pitch+=dy/std::abs(dy);
    }

    
    if (yaw>=180)
        yaw=180;
    else if (yaw<=0)
        yaw=0;

    if (pitch>=180)
        pitch=180;
    else if (pitch<=0)
        pitch=0;
    pos.yaw=static_cast<uint8_t>(yaw);
    pos.pitch=static_cast<uint8_t>(pitch);
    usleep(2000);
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