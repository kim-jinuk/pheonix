#include "running_control_csc/globals.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/sender.hpp"
#include "running_control_csc/BIT.hpp"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>

SystemInfo sysInfo;
StateSync statesync;

Position pos;
std::mutex pos_mtx;

TargetInfo targetInfo;
Cam_opt cam_opt;

std::shared_ptr<FramePacket> latest_pkt;
std::mutex pkt_mtx;

std::vector<std::string> State_str={"CHECKING", "IDLE", "RUNNING"};
std::vector<std::string> Mode_str={"SCAN", "MANUAL", "TRACKING"};

static const std::vector<std::string> CmdFlagStr = {
    "Mode_change",
    "EO/IR_change",
    "Prep_opt",
    "Move_motor",
    "Tracking"
};

const std::unordered_map<CmdFlag, std::vector<std::string>> cmdDict = {
    { CmdFlag::Mode_change,   {"SCAN", "MANUAL"} },
    { CmdFlag::EOIR_change,   {"EO", "IR"} },
    { CmdFlag::Prep_opt,      {"opt1", "opt2", "opt3", "opt4", "opt5"} },
    { CmdFlag::Move_motor,    {"yaw;CW", "yaw;CCW", "pitch;CW", "pitch;CCW"} },
    { CmdFlag::Tracking,      {} }  // Tracking은 ID니까 무시
};


ThreadSafeQueue<FramePtr> enhance_to_infer;
ThreadSafeQueue<FramePtr> infer_to_send;

