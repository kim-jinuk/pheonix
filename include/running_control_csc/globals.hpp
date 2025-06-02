#pragma once

#include <atomic>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>

enum class State {
    CHECKING,
    IDLE,
    RUNNING
};

enum class Mode{
    SCAN,
    MANUAL,
    TRACKING
};

// 구분 되어야 함
struct TcpCommand {
    uint8_t mode_num;
    int8_t dx, dy;
    // int8_t tracking_id;
};

struct Position {
    int yaw = 0;
    int pitch = 0;
};

struct TargetInfo {
    int id;
    int x;
    int y;
};

struct SystemInfo {
    std::atomic<State> current_state=State::CHECKING;
    std::atomic<Mode> current_mode=Mode::MANUAL;
    std::atomic<bool> TCP_connect=false;
    bool TPU_state=false;
    bool CAM_state=false;
    
};

struct StateSync {
    std::mutex mtx;
    std::condition_variable cv;
};


extern StateSync statesync;
extern SystemInfo sysInfo;


extern Position pos;
extern std::mutex pos_mtx;

extern TargetInfo targetInfo;


