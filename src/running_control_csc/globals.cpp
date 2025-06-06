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


std::vector<std::string> State_str={"CHECKING", "IDLE", "RUNNING"};
std::vector<std::string> Mode_str={"SCAN", "MANUAL", "TRACKING"};



