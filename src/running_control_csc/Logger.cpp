
#include "running_control_csc/Logger.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <thread>
#include <filesystem> 



Logger::Logger(const std::string& directory) {
    std::string filename = "log_" + getCurrentTimestampForFile() + ".csv";
    std::filesystem::create_directories(directory); // 디렉토리 없으면 생성
    std::string fullpath = directory + "/" + filename;

    file.open(fullpath, std::ios::out);
    if (file.is_open()) {
        file << "timestamp,mode\n";
        std::cout << "Log file created at: " << fullpath << std::endl;
        sysInfo.logging_enabled=true;
    } 
    else {
        std::cerr << "Failed to open log file: " << fullpath << std::endl;
    }

}
Logger::~Logger() {
    file.close();
}


void Logger::logOperation(const std::string& mode, const std::string& meta, int angle1, int angle2) {
    std::lock_guard<std::mutex> lock(log_mtx);
    if (file.is_open()) {
        file << getCurrentTimestamp() << ",DATA,"
        << mode << "," << meta << "," << angle1 << "," << angle2 << "\n";
        if (file.fail()) {
            std::cerr << "[LOGGER] Disk full. Logging disabled.\n";
            file.close();
            sysInfo.logging_enabled = false;
        }
    }

}

void Logger::logStateChange(const std::string& from, const std::string& to) {
    std::lock_guard<std::mutex> lock(log_mtx);
    if (file.is_open()) {
        file << getCurrentTimestamp() << ",State_CHANGE,,"
         << from << "->" << to << ",,\n";
        if (file.fail()) {
            std::cerr << "[LOGGER] Disk full. Logging disabled.\n";
            file.close();
            sysInfo.logging_enabled = false;
        }
    }
    
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(log_mtx);
    if (file.is_open()) {
        file.flush();
        if (file.fail()) {
            std::cerr << "[LOGGER] Disk full. Logging disabled.\n";
            file.close();
            sysInfo.logging_enabled = false;
        }
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&now_c, &tm_buf);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return std::string(buffer);
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