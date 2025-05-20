#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <algorithm>  // std::clamp 포함

#define MAGIC_WORD 0xA5A5
#define PORT 9999
#define PWMCHIP "/sys/class/pwm/pwmchip0"
#define PERIOD_NS "40000000"  // 20ms 주기 (서보모터 기준)

int angle_to_duty(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    return 39000000 + (angle * (35000000 - 39000000)) / 180;
}

void write_to_file(const std::string& path, const std::string& value) {
    int fd = open(path.c_str(), O_WRONLY);
    if (fd >= 0) {
        write(fd, value.c_str(), value.size());
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
        usleep(200000);  // wait for sysfs to appear
    }
}

void setup_pwm_channel(int ch) {
    std::string base = std::string(PWMCHIP) + "/pwm" + std::to_string(ch);
    write_to_file(base + "/period", PERIOD_NS);
    write_to_file(base + "/enable", "1");
}

void set_duty(int ch, int duty_ns) {
    std::string path = std::string(PWMCHIP) + "/pwm" + std::to_string(ch) + "/duty_cycle";
    write_int(path, duty_ns);
}

int main() {
    // PWM 초기 설정
    export_pwm(0); setup_pwm_channel(0);
    export_pwm(1); setup_pwm_channel(1);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in serv_addr{}, client_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        return 1;
    }

    listen(sockfd, 1);
    std::cout << "🛠️ 서버 대기 중...\n";

    socklen_t len = sizeof(client_addr);
    int client_fd = accept(sockfd, (sockaddr*)&client_addr, &len);
    std::cout << "✅ 클라이언트 연결됨\n";

    int angle_x = 90, angle_y = 90;
    set_duty(0, angle_to_duty(angle_x));
    set_duty(1, angle_to_duty(angle_y));

  uint8_t buf[6];
    while (true) {
        ssize_t n = recv(client_fd, buf, 6, 0);
        if (n != 6) break;

        uint16_t magic = (buf[0] << 8) | buf[1];
        uint16_t modenum = (buf[2] << 8) | buf[3];
        int8_t nx = static_cast<int8_t>(buf[4]);
        int8_t ny = static_cast<int8_t>(buf[5]);

        if (magic != MAGIC_WORD) continue;

        angle_x += nx;
        angle_y += ny;
        angle_x = std::clamp(angle_x, 0, 180);
        angle_y = std::clamp(angle_y, 0, 180);

        std::cout << "↪️ 명령 수신: mode=" << modenum
                  << ", nx=" << (int)nx << ", ny=" << (int)ny
                  << " → X=" << angle_x << "°, Y=" << angle_y << "°\n";

        set_duty(0, angle_to_duty(angle_x));
        set_duty(1, angle_to_duty(angle_y));
    }

    close(client_fd);
    close(sockfd);
    return 0;
}
