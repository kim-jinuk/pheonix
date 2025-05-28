
#include "controlMotor.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mutex>
#include <string>
#include <algorithm>


#define PWMCHIP "/sys/class/pwm/pwmchip0"
#define PERIOD_NS "40000000"

int angle_x = 90;
int angle_y = 90;
std::mutex angle_mutex;
std::mutex motor_mutex;

namespace {

    void write_to_file(const std::string& path, const std::string& value) {
        int fd = open(path.c_str(), O_WRONLY);
        if (fd >= 0) {
            (void)write(fd, value.c_str(), value.size());
            close(fd);
        } else {
            perror(path.c_str());
        }
    }

    void write_int(const std::string& path, int value) {
        write_to_file(path, std::to_string(value));
    }

    void export_pwm(int ch) {
        std::string pwm_path = std::string(PWMCHIP) + "/pwm" + std::to_string(ch);
        if (access(pwm_path.c_str(), F_OK) != 0) {
            write_int(PWMCHIP "/export", ch);
            usleep(200000);
        }
    }

    void setup_pwm_channel(int ch) {
        std::string base = std::string(PWMCHIP) + "/pwm" + std::to_string(ch);
        write_to_file(base + "/period", PERIOD_NS);
        write_to_file(base + "/enable", "1");
    }
}

int AngleToDuty(int angle) {
    return 39000000 + (std::clamp(angle, 0, 180) * (35000000 - 39000000)) / 180;
}

void SetDuty(int ch, int duty_ns) {
    std::string path = std::string(PWMCHIP) + "/pwm" + std::to_string(ch) + "/duty_cycle";
    write_int(path, duty_ns);
}

void InitMotor() {
    export_pwm(0); setup_pwm_channel(0);
    export_pwm(1); setup_pwm_channel(1);
    SetDuty(0, AngleToDuty(angle_x));
    SetDuty(1, AngleToDuty(angle_y));
}

void UpdateMotor(int dx, int dy) {
    std::lock_guard<std::mutex> lock(motor_mutex);
    angle_x = std::clamp(angle_x + dx, 0, 180);
    angle_y = std::clamp(angle_y + dy, 0, 180);
    SetDuty(0, AngleToDuty(angle_x));
    SetDuty(1, AngleToDuty(angle_y));
}

int GetMotorX() {
    std::lock_guard<std::mutex> lock(motor_mutex);
    return angle_x;
}

int GetMotorY() {
    std::lock_guard<std::mutex> lock(motor_mutex);
    return angle_y;
}
