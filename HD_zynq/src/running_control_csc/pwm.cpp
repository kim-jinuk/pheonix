// pwm.cc
// pwm.h에 선언된 함수 구현부
//
// - 리눅스 sysfs 기반 pwm 인터페이스를 사용하여 
//   두 개의 PWM 채널(pwmchip0/pwm0 → Yaw, pwmchip1/pwm0 → Pitch)을 제어합니다.
// - MG996R 서보 기준으로 20 ms 주기, 1~2 ms 펄스폭을 각도(0~180°)로 매핑합니다.

#include "running_control_csc/pwm.hpp"
// pwm.cc

#include <chrono>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>  // usleep, access
#include <sys/stat.h> // stat

static constexpr int   PWM_PERIOD_NS = 20'000'000;   // 20 ms = 20 000 000 ns
static constexpr int   SERVO_MIN_PULSE_NS = 610'000;      //  1 ms =  1 000 000 ns (각도 0°)
static constexpr int   SERVO_MAX_PULSE_NS = 2'500'000;    //  2 ms =  2 000 000 ns (각도 180°)
static constexpr int   SERVO_MAX_ANGLE = 180;          // 서보 각도 최대 (180°)
static constexpr int   EXPORT_WAIT_US = 100'000;      // export 후 sysfs 생성 대기 (100 ms)

static constexpr int   YAW_CHIP_NUM = 0;  // pwmchip0
static constexpr int   YAW_CHANNEL_NUM = 0;  // pwm0

static constexpr int   PITCH_CHIP_NUM = 1;  // pwmchip1
static constexpr int   PITCH_CHANNEL_NUM = 0;  // pwm0

static bool writeSysfs(const std::string& path, const std::string& value) {
    std::ofstream fs(path);
    if (!fs.is_open()) {
        std::cerr << "[ERROR] Cannot open sysfs path: " << path
            << " (" << std::strerror(errno) << ")\n";
        return false;
    }
    fs << value;
    if (fs.fail()) {
        std::cerr << "[ERROR] Failed to write '" << value << "' to " << path
            << " (" << std::strerror(errno) << ")\n";
        return false;
    }
    fs.close();
    return true;
}

static bool pathExists(const std::string& path) {
    struct stat sb;
    return (stat(path.c_str(), &sb) == 0);
}

static std::string pwmBasePath(int chip, int channel) {
    // 예: "/sys/class/pwm/pwmchip0/pwm0"
    std::ostringstream ss;
    ss << "/sys/class/pwm/pwmchip" << chip << "/pwm" << channel;
    return ss.str();
}

static std::string pwmChipPath(int chip) {
    // 예: "/sys/class/pwm/pwmchip0"
    std::ostringstream ss;
    ss << "/sys/class/pwm/pwmchip" << chip;
    return ss.str();
}

static int angleToPulseNs(int angle) {
    if (angle < 0) angle = 0;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    // 0° → 1 000 000 ns, 180° → 2 000 000 ns
    // linear interpolation: pulse = MIN + (angle / 180) * (MAX - MIN)
    return SERVO_MIN_PULSE_NS +
        static_cast<int>((static_cast<long long>(angle) *
            (SERVO_MAX_PULSE_NS - SERVO_MIN_PULSE_NS)) /
            SERVO_MAX_ANGLE);
}

static bool setupSinglePwm(int chip, int channel, int initial_angle) {
    const std::string chip_path = pwmChipPath(chip);           // ex: "/sys/class/pwm/pwmchip0"
    const std::string pwm_path = pwmBasePath(chip, channel);  // ex: "/sys/class/pwm/pwmchip0/pwm0"
    const std::string export_path = chip_path + "/export";       // ex: "/sys/class/pwm/pwmchip0/export"
    const std::string unexport_path = chip_path + "/unexport";   // ex: "/sys/class/pwm/pwmchip0/unexport"

    // 1) pwmX 디렉터리가 이미 있는지 확인 → 없다면 export
    if (!pathExists(pwm_path)) {
        if (!writeSysfs(export_path, std::to_string(channel))) {
            std::cerr << "[ERROR] Failed to export PWM channel: chip"
                << chip << "/pwm" << channel << "\n";
            return false;
        }
        usleep(EXPORT_WAIT_US);
    }

    // 2) period 설정 (20 000 000 ns)
    const std::string period_path = pwm_path + "/period";
    if (!writeSysfs(period_path, std::to_string(PWM_PERIOD_NS))) {
        std::cerr << "[ERROR] Failed to set period for pwmchip"
            << chip << "/pwm" << channel << "\n";
        return false;
    }

    // 3) duty_cycle 설정 (angle → ns)
    int pulse_ns = angleToPulseNs(initial_angle);
    const std::string duty_path = pwm_path + "/duty_cycle";
    if (!writeSysfs(duty_path, std::to_string(pulse_ns))) {
        std::cerr << "[ERROR] Failed to set duty_cycle for pwmchip"
            << chip << "/pwm" << channel << "\n";
        return false;
    }

    // 4) enable = 1 (PWM 동작 시작)
    const std::string enable_path = pwm_path + "/enable";
    if (!writeSysfs(enable_path, "1")) {
        std::cerr << "[ERROR] Failed to enable PWM channel: chip"
            << chip << "/pwm" << channel << "\n";
        return false;
    }

    return true;
}

static bool updateDuty(int chip, int channel, int angle) {
    const std::string pwm_path = pwmBasePath(chip, channel);
    const std::string duty_path = pwm_path + "/duty_cycle";

    int pulse_ns = angleToPulseNs(angle);
    if (!writeSysfs(duty_path, std::to_string(pulse_ns))) {
        std::cerr << "[ERROR] Failed to update duty_cycle for pwmchip"
            << chip << "/pwm" << channel << "\n";
        return false;
    }
    return true;
}

void pwm_init() {
    // Yaw 서보: pwmchip0/pwm0, 초기 각도  90°
    // Pitch 서보: pwmchip1/pwm0, 초기 각도   90°
    bool ok_yaw = setupSinglePwm(YAW_CHIP_NUM, YAW_CHANNEL_NUM, 90);
    bool ok_pitch = setupSinglePwm(PITCH_CHIP_NUM, PITCH_CHANNEL_NUM, 90);

    if (!ok_yaw) {
        std::cerr << "[FATAL] Yaw PWM initialization failed\n";
    }
    if (!ok_pitch) {
        std::cerr << "[FATAL] Pitch PWM initialization failed\n";
    }
}

void pwm(int yaw_angle, int pitch_angle) {

    int yaw = yaw_angle;
    int pitch = pitch_angle;
    if (yaw < 0)   yaw = 0;
    if (yaw > 180) yaw = 180;
    if (pitch < 0)   pitch = 0;
    if (pitch > 180) pitch = 180;

    bool ok_yaw = updateDuty(YAW_CHIP_NUM, YAW_CHANNEL_NUM, yaw);
    bool ok_pitch = updateDuty(PITCH_CHIP_NUM, PITCH_CHANNEL_NUM, pitch);

    if (!ok_yaw) {
        std::cerr << "[ERROR] Failed to set Yaw angle to " << yaw << "°\n";
    }
    if (!ok_pitch) {
        std::cerr << "[ERROR] Failed to set Pitch angle to " << pitch << "°\n";
    }
}
