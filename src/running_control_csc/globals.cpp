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
std::unordered_map<int, StableLabel> m_label_state;

const std::unordered_set<std::string> allowed_labels = {
    "person", "airplane", "bus", "truck", "car"
};
ThreadSafeQueue<FramePtr> enhance_to_infer(INFER_QUEUE_SIZE);
std::vector<InferenceResult> InferResult;
std::mutex infer_mtx;

ThreadSafeQueue<SendPacket> send_queue(SEND_QUEUE_SIZE);

