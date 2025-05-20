#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <mutex>
#include <condition_variable>
#include <algorithm>

#define DEVICE "/dev/video0"
#define WIDTH 640
#define HEIGHT 480
#define BUFFER_COUNT 2
#define MAX_PACKET_SIZE 1400
#define HEADER_SIZE 8
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)
#define MAGIC_WORD 0xA5A5
#define PWMCHIP "/sys/class/pwm/pwmchip0"
#define PERIOD_NS "40000000"

using namespace std;

/**
 * @brief 현재 모터의 X축 각도
 * @details -90 ~ 90 범위 내에서 이동 가능
 */
int8_t current_angle_x = 0;

/**
 * @brief 현재 모터의 Y축 각도
 * @details -90 ~ 90 범위 내에서 이동 가능
 */
int8_t current_angle_y = 0;

int angle_x = 90, angle_y = 90;
bool tcp_connected = false;
mutex angle_mutex;
mutex conn_mutex;
condition_variable conn_cv;

string UDP_IP;
int UDP_PORT;
int TCP_PORT;
int ANGLE_TCP_PORT;

// ───────────────── 설정 읽기 ─────────────────
/**
 * @brief 설정 파일(config.txt)에서 key-value 형식으로 값을 읽어 map으로 반환
 */
map<string, string> load_config(const string& filename) {
    ifstream file(filename);
    map<string, string> config;
    string line;

    while (getline(file, line)) {
        auto pos = line.find('=');
        if (pos != string::npos) {
            string key = line.substr(0, pos);
            string val = line.substr(pos + 1);
            config[key] = val;
        }
    }
    return config;
}

// ───────────────── UDP 영상 전송 ─────────────────
/**
 * @brief MJPEG 영상 데이터를 V4L2를 통해 캡처 후 UDP로 전송
 * @param ip 전송 대상 IP
 * @param port 전송 대상 포트
 */
void udp_sender(const string& ip, int port) {
    cout << "[UDP] 영상 전송 시작: " << ip << ":" << port << endl;
    // TODO: V4L2 초기화 및 영상 캡처 후 UDP 전송 구현
}

// ───────────────── TCP 제어 수신 ─────────────────
/**
 * @brief TCP를 통해 클라이언트 명령을 수신하고 모터 각도 제어
 * @param port 수신 포트
 */
void tcp_receiver(int port) {
    cout << "[TCP] 제어 서버 대기 중: 포트 " << port << endl;
    // TODO: TCP 연결 수신 및 PWM 각도 제어 구현
}

// ───────────────── 각도 주기 송신 ─────────────────
/**
 * @brief 현재 각도를 TCP로 주기적으로 송신 (0.5초 간격)
 * @param ip 클라이언트 IP
 * @param port 송신 포트
 */
void angle_sender(const string& ip, int port) {
    cout << "[TCP] 각도 전송 시작: " << ip << ":" << port << endl;
    // TODO: TCP 연결 후 current_angle_x/y 주기 전송 구현
}

// ───────────────── 메인 ─────────────────
int main() {
    // 설정값 로딩
    auto config = load_config("config.txt");
    UDP_IP = config["UDP_IP"];
    UDP_PORT = stoi(config["UDP_PORT"]);
    TCP_PORT = stoi(config["TCP_PORT"]);
    ANGLE_TCP_PORT = stoi(config["ANGLE_TCP_PORT"]);

    // 각 기능 스레드 실행
    thread udpThread(udp_sender, UDP_IP, UDP_PORT);
    thread tcpThread(tcp_receiver, TCP_PORT);
    thread angleThread(angle_sender, UDP_IP, ANGLE_TCP_PORT);

    // 스레드 종료 대기
    udpThread.join();
    tcpThread.join();
    angleThread.join();

    return 0;
}
