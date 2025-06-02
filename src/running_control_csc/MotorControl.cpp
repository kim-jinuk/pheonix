
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/pwm.hpp"
#include <iostream>

#define MOTOR
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
    static const int STEP = 5;

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
void ManualMotor::enqueueDelta(int dx, int dy) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        delta_queue.push({dx, dy});
}

void ManualMotor::updateAngle() {
    std::cout << "MANUAL logic"<<std::endl;

    {
        std::lock_guard<std::mutex> lock(queue_mtx);

        if (!delta_queue.empty()) 
        {
            auto [dx, dy] = delta_queue.front();
            pos.yaw+=dx;
            pos.pitch+=dy;
            delta_queue.pop();
        }

    }

    if (pos.yaw>=180)
        pos.yaw=180;
    if (pos.yaw<=0)
        pos.yaw=0;
}

/* 
    Tracking
*/
void TrackingMotor::updateAngle() {
     std::cout << "TRACKING logic"<<std::endl;
    

}


void MotorControl::init_pos() {
    std::cout << "init motor"<<std::endl;
   // pwm_init();
}

void MotorControl::move() {
    std::cout << "move motor"<<std::endl;
    std::cout << "Yaw:" <<pos.yaw << " Pitch:" << pos.pitch <<std::endl;
   // pwm(pos.yaw,pos.pitch);
}

void MotorControl::setStrategy(int new_mode) {
    

    if (current_type == new_mode) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(strategy_mtx);
        switch (new_mode) {
            case static_cast<int>(Mode::SCAN):
                strategy = std::make_shared<SCANMotor>();
                break;
            case static_cast<int>(Mode::MANUAL):
                strategy = std::make_shared<ManualMotor>();
                break;
            case static_cast<int>(Mode::TRACKING):
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
void MotorControl::enqueueDeltaIfManual(int dx, int dy) {

    std::lock_guard<std::mutex> lock(strategy_mtx);
        if (current_type == static_cast<int>(Mode::MANUAL)) {
            if (auto manual = std::dynamic_pointer_cast<ManualMotor>(strategy)) {
                manual->enqueueDelta(dx, dy);
            }
        }
    
}