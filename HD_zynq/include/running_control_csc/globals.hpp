#pragma once

#include <atomic>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#define TCP_MAGIC_WORD 0xA5A5

enum class State: uint8_t {
    CHECKING,
    IDLE,
    RUNNING
};

enum class Mode : uint8_t{
    SCAN,
    MANUAL,
    TRACKING,
    DEFAULT
};

struct FrameData {

    uint8_t frame_id;
   // cv::Mat enhanced_frame;
    // meta data 추가
};

struct Cam_opt {
    std::atomic<uint8_t> eo_ir;
    std::atomic<uint8_t> opt1;
    std::atomic<uint8_t> opt2;
    std::atomic<uint8_t> opt3;
    std::atomic<uint8_t> opt4;
    std::atomic<uint8_t> opt5;
    void fromCmd(uint8_t cmd) {
        opt1.store((cmd & 0x01) != 0);
        opt2.store((cmd & 0x02) != 0);
        opt3.store((cmd & 0x04) != 0);
        opt4.store((cmd & 0x08) != 0);
        opt5.store((cmd & 0x10) != 0);
    }
};

// 4byte
struct  __attribute__((packed)) TcpCommand {

    uint16_t  magic_word;
    uint8_t cmd_flag;
    uint8_t cmd;
};

enum {
    Mode_num, Cam_num,Prep_opt,move_motor,track
};
// 8byte

struct  __attribute__((packed)) TcpState {
    uint16_t magic_word=TCP_MAGIC_WORD;
    uint8_t state_num;
    uint8_t mode_num;
    uint8_t Nx=90; 
    uint8_t Ny=90;
    uint8_t tpu;
    uint8_t cam;
    uint8_t sdcard;
    bool operator==(const TcpState& other) const {
        return magic_word == other.magic_word &&
               state_num   == other.state_num &&
               mode_num    == other.mode_num &&
               Nx          == other.Nx &&
               Ny          == other.Ny &&
               tpu         == other.tpu &&
               cam         == other.cam &&
               sdcard      == other.sdcard;
    }
    bool operator!=(const TcpState& other) const {
        return !(*this == other);
    }
};


struct Position {
    int yaw =90;
    int pitch = 90;
};

struct TargetInfo {
    std::atomic<uint8_t> id;

    int x;
    int y;
    mutable std::mutex mtx;
public:
    void setXY(int new_x, int new_y) {
        std::lock_guard<std::mutex> lock(mtx);
        x = new_x;
        y = new_y;
    }

    std::pair<int, int> getXY() const {
        std::lock_guard<std::mutex> lock(mtx);
        return {x, y};
    }
};

struct SystemInfo {
    std::atomic<State> current_state=State::CHECKING; // share
    std::atomic<Mode> current_mode=Mode::MANUAL; // share
    std::atomic<bool> TCP_state_connected=false; // share?
    std::atomic<bool> TCP_cmd_connected=false; //share
    bool TPU_state=false; // not share
    bool CAM_state=false; // not share
    bool logging_enabled=false; // share?
};

struct StateSync {
    std::mutex mtx;
    std::condition_variable cv;
};


extern std::vector<std::string> State_str;
extern std::vector<std::string> Mode_str;

extern StateSync statesync;
extern SystemInfo sysInfo;

extern Cam_opt cam_opt;

extern Position pos;
extern std::mutex pos_mtx;

extern TargetInfo targetInfo;


