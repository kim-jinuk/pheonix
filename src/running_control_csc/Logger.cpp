
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
    std::lock_guard<std::mutex> lock(log_mtx);

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

std::string Logger::getCurrentTimestampForFile() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&now_c, &tm_buf);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm_buf);
    return std::string(buffer);
}