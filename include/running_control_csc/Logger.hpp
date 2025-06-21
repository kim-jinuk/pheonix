
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
    std::mutex cmd_mtx;
    uint32_t flushtime=0;
public :

    Logger(const std::string& directory = "./");
    ~Logger();
    void logOperation(const std::string& mode, const std::string& meta, int angle1, int angle2);

    void logCmd(uint8_t flag, uint8_t cmd_val);

    void logStateChange(const std::string& from, const std::string& to);
    void flush();
    void logMeta(uint32_t frame_id,const std::string& timestamp ,const std::vector<ObjectInfo>& objects);

    std::string getCurrentTimestamp();
    std::string getCurrentTimestampForFile();

};