#include "running_control_csc/globals.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/sender.hpp"
#include "running_control_csc/BIT.hpp"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>


SystemInfo sysInfo;
StateSync statesync;

Position pos;
std::mutex pos_mtx;

TargetInfo targetInfo;




