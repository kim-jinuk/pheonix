#include "running_control_csc/globals.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/sender.hpp"
#include "running_control_csc/BIT.hpp"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#include <map>
#include <unordered_set>

SystemInfo sysInfo;
StateSync statesync;

Position pos;
std::mutex pos_mtx;

TargetInfo targetInfo;
Cam_opt cam_opt;


std::vector<std::string> State_str={"CHECKING", "IDLE", "RUNNING"};
std::vector<std::string> Mode_str={"SCAN", "MANUAL", "TRACKING"};

std::unordered_map<int,std::string> m_track_label;

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



const std::unordered_set<std::string> allowed_labels = {
    "person", "airplane", "bus", "truck", "car"
};
ThreadSafeQueue<FramePtr> enhance_to_infer(INFER_QUEUE_SIZE);
std::vector<InferenceResult> InferResult;
std::mutex infer_mtx;

ThreadSafeQueue<SendPacket> send_queue(SEND_QUEUE_SIZE);