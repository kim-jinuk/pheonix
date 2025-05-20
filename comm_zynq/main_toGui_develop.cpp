#include <iostream>
#include <fstream>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <map>

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

int angle_x = 90, angle_y = 90;  ///< PWM으로 제어할 현재 각도 값
mutex angle_mutex;              ///< angle_x/y 보호용 뮤텍스
bool tcp_connected = false;     ///< TCP 연결 여부
mutex conn_mutex;               ///< 연결 상태 보호용 뮤텍스
condition_variable conn_cv;     ///< TCP 연결 감지를 위한 조건 변수

// 설정값 (config.txt에서 읽어옴)
string UDP_IP;
int UDP_PORT;
int TCP_PORT;
int ANGLE_TCP_PORT;

/**
 * @brief 설정 파일(config.txt)에서 key-value 형식의 설정값을 읽어 map으로 반환
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

/**
 * @brief 각도를 PWM duty 값으로 변환
 */
int angle_to_duty(int angle) {
    return 39000000 + (clamp(angle, 0, 180) * (35000000 - 39000000)) / 180;
}

/**
 * @brief 파일에 문자열을 씀
 */
void write_to_file(const string& path, const string& value) {
    int fd = open(path.c_str(), O_WRONLY);
    if (fd >= 0) {
        (void)write(fd, value.c_str(), value.size());
        close(fd);
    } else {
        perror(path.c_str());
    }
}

/**
 * @brief 파일에 정수값을 문자열로 변환해 씀
 */
void write_int(const string& path, int value) {
    write_to_file(path, to_string(value));
}

/**
 * @brief PWM 채널 export
 */
void export_pwm(int ch) {
    string pwm_path = string(PWMCHIP) + "/pwm" + to_string(ch);
    if (access(pwm_path.c_str(), F_OK) != 0) {
        write_int(PWMCHIP "/export", ch);
        usleep(200000);
    }
}

/**
 * @brief PWM 채널 설정 (주기 및 enable)
 */
void setup_pwm_channel(int ch) {
    string base = string(PWMCHIP) + "/pwm" + to_string(ch);
    write_to_file(base + "/period", PERIOD_NS);
    write_to_file(base + "/enable", "1");
}

/**
 * @brief PWM duty 설정
 */
void set_duty(int ch, int duty_ns) {
    string path = string(PWMCHIP) + "/pwm" + to_string(ch) + "/duty_cycle";
    write_int(path, duty_ns);
}

/**
 * @brief MJPEG 카메라 프레임을 캡처하여 UDP로 전송
 */
void udp_sender() {
    struct Buffer { void* start; size_t length; };

    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) { perror("[ERR] open camera"); return; }

    struct v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    ioctl(fd, VIDIOC_S_FMT, &fmt);

    struct v4l2_requestbuffers req{};
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_REQBUFS, &req);

    Buffer buffers[BUFFER_COUNT];
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ioctl(fd, VIDIOC_QUERYBUF, &buf);
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    }

    for (int i = 0; i < BUFFER_COUNT; ++i) {
        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = inet_addr(UDP_IP.c_str());

    uint32_t frame_id = 0;
    cout << "[UDP] video sender initialized.\n";

    while (true) {
        unique_lock<mutex> lock(conn_mutex);
        conn_cv.wait(lock, [] { return tcp_connected; });
        lock.unlock();

        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) continue;

        unsigned char* data = (unsigned char*)buffers[buf.index].start;
        size_t size = buf.bytesused;
        int total_packets = (size + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;

        for (int i = 0; i < total_packets; ++i) {
            lock_guard<mutex> guard(conn_mutex);
            if (!tcp_connected) break;

            int offset = i * MAX_PAYLOAD_SIZE;
            int payload_size = min((int)(size - offset), MAX_PAYLOAD_SIZE);

            uint32_t fid_net = htonl(frame_id);
            uint16_t pid_net = htons(i);
            uint16_t tpkts_net = htons(total_packets);
            unsigned char packet[MAX_PACKET_SIZE];
            memcpy(packet, &fid_net, 4);
            memcpy(packet + 4, &pid_net, 2);
            memcpy(packet + 6, &tpkts_net, 2);
            memcpy(packet + HEADER_SIZE, data + offset, payload_size);

            sendto(sock, packet, payload_size + HEADER_SIZE, 0, (sockaddr*)&addr, sizeof(addr));
            usleep(1000);
        }

        ioctl(fd, VIDIOC_QBUF, &buf);
        frame_id++;
    }

    close(sock);
    close(fd);
}

/**
 * @brief 클라이언트로부터 dx/dy 제어 명령을 수신하고 각도 및 PWM을 조정
 */
void tcp_receiver() {
    export_pwm(0); setup_pwm_channel(0);
    export_pwm(1); setup_pwm_channel(1);
    set_duty(0, angle_to_duty(angle_x));
    set_duty(1, angle_to_duty(angle_y));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{}, client_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(TCP_PORT);

    bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(sockfd, 1);
    cout << "[TCP] motor control server ready..\n";

    while (true) {
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(sockfd, (sockaddr*)&client_addr, &len);
        if (client_fd < 0) {
            perror("[ERR] accept failed");
            continue;
        }
        {
            lock_guard<mutex> lock(conn_mutex);
            tcp_connected = true;
        }
        conn_cv.notify_all();

        cout << "[TCP] motor control client connected.\n";

        uint8_t buf[6];
        while (true) {
            ssize_t n = recv(client_fd, buf, 6, 0);
            if (n != 6) {
                cout << "[TCP] client disconnected.\n";
                {
                    lock_guard<mutex> lock(conn_mutex);
                    tcp_connected = false;
                }
                conn_cv.notify_all();
                close(client_fd);
                break;
            }

            uint16_t magic = (buf[0] << 8) | buf[1];
            uint16_t modenum = (buf[2] << 8) | buf[3];
            int8_t dx = static_cast<int8_t>(buf[4]);
            int8_t dy = static_cast<int8_t>(buf[5]);

            if (magic != MAGIC_WORD) continue;

            {
                lock_guard<mutex> lock(angle_mutex);
                angle_x = clamp(angle_x + dx, 0, 180);
                angle_y = clamp(angle_y + dy, 0, 180);
                set_duty(0, angle_to_duty(angle_x));
                set_duty(1, angle_to_duty(angle_y));
            }

            cout << "[RCV] mode=" << modenum << ", dx=" << (int)dx
                 << ", dy=" << (int)dy << " → angle=(" << angle_x << ", " << angle_y << ")\n";
        }
    }

    close(sockfd);
}

/**
 * @brief 현재 각도를 0.5초 간격으로 TCP 클라이언트에 전송
 */
void angle_sender(const string& client_ip) {
    while (true) {
        unique_lock<mutex> lock(conn_mutex);
        conn_cv.wait(lock, [] { return tcp_connected; });
        lock.unlock();

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { perror("[ERR] angle_sender socket"); sleep(1); continue; }

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(ANGLE_TCP_PORT);
        inet_pton(AF_INET, client_ip.c_str(), &serv_addr.sin_addr);

        if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            cerr << "[WARN] angle sender server binding fail.. ready ...\n";
            close(sock);
            sleep(1);
            continue;
        }

        cout << "[TCP] angle sender server binding.\n";

        while (true) {
            {
                lock_guard<mutex> lock(conn_mutex);
                if (!tcp_connected) break;
            }

            uint8_t packet[6];
            {
                lock_guard<mutex> lock(angle_mutex);
                packet[0] = 0xA5;
                packet[1] = 0x5A;
                packet[2] = angle_x;
                packet[3] = angle_y;
                packet[4] = 0x0D;
                packet[5] = 0x0A;
            }

            if (send(sock, packet, 6, 0) <= 0) break;
            usleep(500 * 1000);
        }

        close(sock);
        sleep(1);
    }
}

/**
 * @brief 프로그램 진입점. 설정 로드 후 모든 스레드 시작
 */
int main() {
    auto config = load_config("config.txt");
    UDP_IP = config["UDP_IP"];
    UDP_PORT = stoi(config["UDP_PORT"]);
    TCP_PORT = stoi(config["TCP_PORT"]);
    ANGLE_TCP_PORT = stoi(config["ANGLE_TCP_PORT"]);

    thread udpThread(udp_sender);
    thread tcpThread(tcp_receiver);
    thread angleThread(angle_sender, UDP_IP);

    udpThread.join();
    tcpThread.join();
    angleThread.join();
    return 0;
}