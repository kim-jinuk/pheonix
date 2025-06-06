#pragma once
#include <queue>
#include <memory>
#include "running_control_csc/globals.hpp"
/* === Motor 전략 인터페이스 === */
class IMotorStrategy {
public:
    virtual ~IMotorStrategy() {}
    // ModeManager를 넘겨받아 상태를 갱신
    virtual void updateAngle() = 0;
};

/* === SCAN 전략 === */
class SCANMotor : public IMotorStrategy {
    
public:
    SCANMotor();
    ~SCANMotor();
    int direction=-1;
    int sweep_low;
    int sweep_high;
    void updateAngle() override;
    
    
};

/* === MANUAL 전략 === */
class ManualMotor : public IMotorStrategy {

private:
    std::queue<uint8_t> delta_queue;
    std::mutex queue_mtx;
public:
    ManualMotor();
    ~ManualMotor();
    void updateAngle() override;
    void enqueueDelta(uint8_t delta);
};

/* === TRACKING 전략 === */
class TrackingMotor : public IMotorStrategy {

public:
    void updateAngle() override;
};

class MotorControl {

private:
    std::shared_ptr<IMotorStrategy> strategy;
    std::mutex strategy_mtx;
    int current_type=static_cast<int>(Mode::MANUAL);
public :
    void init_pos();
    void move();
    void setStrategy(uint8_t new_mode);
    void runStrategy();
    void enqueueDeltaIfManual(uint8_t delta);
};