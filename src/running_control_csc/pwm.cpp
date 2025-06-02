#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "device_control_csc/pwm.hpp"

#define PWMCHIP "/sys/class/pwm/pwmchip0"
#define PERIOD_NS "40000000"  // 20ms

static int angle_to_duty(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    return 39000000 + (angle * (35000000 - 39000000)) / 180;
}

static void write_to_file(const std::string& path, const std::string& value) {
    int fd = open(path.c_str(), O_WRONLY);
    if (fd >= 0) {
        write(fd, value.c_str(), value.size());
        close(fd);
    } else {
        perror(path.c_str());
    }
}

static void write_int(const std::string& path, int value) {
    write_to_file(path, std::to_string(value));
}

static void export_pwm(int ch) {
    std::string pwm_path = std::string(PWMCHIP) + "/pwm" + std::to_string(ch);
    if (access(pwm_path.c_str(), F_OK) != 0) {
        write_int(PWMCHIP "/export", ch);
        usleep(200000);  // sysfs 생성 대기
    }
}

static void setup_pwm_channel(int ch) {
    std::string base = std::string(PWMCHIP) + "/pwm" + std::to_string(ch);
    write_to_file(base + "/period", PERIOD_NS);
    write_to_file(base + "/enable", "1");
}

static void set_duty(int ch, int duty_ns) {
    std::string path = std::string(PWMCHIP) + "/pwm" + std::to_string(ch) + "/duty_cycle";
    write_int(path, duty_ns);
}

void pwm_init() {
    export_pwm(0); setup_pwm_channel(0);
    export_pwm(1); setup_pwm_channel(1);
    pwm(90, 0);  // 초기 위치
}

void pwm(int angle_x, int angle_y) {
    set_duty(0, angle_to_duty(angle_x));
    set_duty(1, angle_to_duty(angle_y));
}
