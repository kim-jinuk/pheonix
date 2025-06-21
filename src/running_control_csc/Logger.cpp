
#include "running_control_csc/Logger.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <thread>
#include <filesystem> 
#include <sstream>  


Logger::Logger(const std::string& directory) {
    std::string ts = getCurrentTimestampForFile();
    std::string filename_cmd  = directory + "/EOIR_CMD_"  + ts + ".csv";
    std::string filename_meta = directory + "/EOIR_META_" + ts + ".csv";
    std::filesystem::create_directories(directory); // 디렉토리 없으면 생성
    
    file_cmd.open(filename_cmd);
    file_meta.open(filename_meta);

    if (!file_cmd.is_open() || !file_meta.is_open()) {
        throw std::runtime_error("Failed to open log files");
    }
    file_meta << "Time,FrameID,cls,id,x,y,w,h,conf\n";
    file_cmd << "Type,Time,cmd_flag,cmd\n";
}

Logger::~Logger() {
    file_cmd.close();
    file_meta.close();
}



void Logger::logOperation(const std::string& mode, const std::string& meta, int angle1, int angle2) {
    std::lock_guard<std::mutex> lock(log_mtx);
   

}

void Logger::logStateChange(const std::string& from, const std::string& to) {
    std::lock_guard<std::mutex> lock(log_mtx);
   
    
}

void Logger::flush() {
    flushtime++;
    if (flushtime%10!=0) return;
    
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        if (file_meta.is_open()) {
            file_meta.flush();
        }
    }

    {
        std::lock_guard<std::mutex> lock(cmd_mtx);
        if (file_cmd.is_open()) {
            file_cmd.flush();
        }
    }
}

std::string Logger::getCurrentTimestamp() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::time_t t = system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setw(3) << std::setfill('0') << ms.count();

    return oss.str();
}

void Logger::logCmd(uint8_t flag, uint8_t cmd_val) {
    std::lock_guard<std::mutex> lock(cmd_mtx);

    static const std::vector<std::string> flag_names = {
        "Mode_num", "Cam_num", "Prep_opt", "MoveMotor", "Track", "InitMotor"
    };

    std::string flag_str = (flag < flag_names.size()) ? flag_names[flag] : "Unknown";

    file_cmd << "[CMD],"
             << getCurrentTimestamp() << ","
             << flag_str << ","
             << static_cast<int>(cmd_val) << "\n";
}

void Logger::logMeta(uint32_t frame_id,const std::string& timestamp ,const std::vector<ObjectInfo>& objects) {
    if (objects.empty()) return;
    {
        std::lock_guard<std::mutex> lock(log_mtx);

        //std::string timestamp = getCurrentTimestamp();  // 한 번만 호출
        bool first = true;

        for (const auto& obj : objects) {
            if (first) {
                file_meta << timestamp << ",";
                first = false;
            } else {
                file_meta << ",";  // timestamp 생략
            }

            file_meta << static_cast<int>(frame_id) << ","
                    << static_cast<int>(obj.cls) << ","
                    << static_cast<int>(obj.tracking_id) << ","
                    << obj.x << ","
                    << obj.y << ","
                    << obj.w << ","
                    << obj.h << ","
                    << obj.conf << "\n";
        }
    }
}


std::string Logger::getCurrentTimestampForFile() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&now_c, &tm_buf);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm_buf);
    return std::string(buffer);
}