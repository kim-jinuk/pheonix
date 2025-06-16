
#pragma once

#include <mutex>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include "running_control_csc/globals.hpp"

class Logger {

private :
    std::ofstream file_cmd;
    std::ofstream file_meta;
    std::mutex log_mtx;

public :
    Logger(const std::string& directory = "./");
    ~Logger();
    void logOperation(const std::string& mode, const std::string& meta, int angle1, int angle2);
    void logCmd(CmdFlag flag, uint8_t cmd_val);
    void logStateChange(const std::string& from, const std::string& to);
    void flush();
    std::string getCurrentTimestamp();
    std::string getCurrentTimestampForFile();

};