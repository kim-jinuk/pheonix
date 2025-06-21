#pragma once

#include <atomic>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#include <opencv2/opencv.hpp>
#include <unordered_set>
#define TCP_MAGIC_WORD 0xA5A5
#define MISSTARGET 9999
#define INFER_PER_FRAME 5
/**
    State enum class
*/
enum class State: uint8_t {
    CHECKING,
    IDLE,
    RUNNING
};

/**
    Mode enum class
*/
enum class Mode : uint8_t{
    SCAN,
    MANUAL,
    TRACKING,
    DEFAULT
};

/**
    image result
*/
struct FrameData {

    uint32_t frame_id;
    cv::Mat img_bgr;
    std::string timestamp;
};

/**
    inference result
*/
struct InferenceResult {
  std::string candidate;
  float score;
  float x1;
  float y1;
  float x2;
  float y2;
};
/**
    Tracking result
*/
struct Track {
    int id;
    cv::KalmanFilter kf;
    cv::Rect2f bbox;
    int age = 0;
    int time_since_update = 0;
};

/**
    cam option
*/
struct Cam_opt {
    std::atomic<uint8_t> eo_ir=0;
    std::atomic<uint8_t> enhance_edges=0;
    std::atomic<uint8_t> enhance_contrast=0;
    std::atomic<uint8_t> enhance_dehaze=0;
//    std::atomic<uint8_t> opt5;
    void fromCmd(uint8_t cmd) {
        enhance_edges.store((cmd & 0x01) != 0);
        enhance_contrast.store((cmd & 0x02) != 0);
        enhance_dehaze.store((cmd & 0x04) != 0);
     //   opt5.store((cmd & 0x10) != 0);
    }
};

/**
    TCP cmd format
*/
struct  __attribute__((packed)) TcpCommand {

    uint16_t  magic_word;
    uint8_t cmd_flag;
    uint8_t cmd;
};

/**
    for TCP cmd foramt
*/
enum {
    Mode_num, Cam_num,Prep_opt,move_motor,track,InitMotor
};
/**
    for sending upd
*/
struct __attribute__((packed)) ObjectInfo {
    uint8_t cls;          // 클래스 ID
    uint8_t tracking_id;  // 트래킹 ID
    int16_t x;            // 박스 좌상단 x
    int16_t y;            // 박스 좌상단 y
    int16_t w;            // 너비
    int16_t h;            // 높이
    float conf;           // confidence 점수
};


/**
    for sending state
*/
struct  __attribute__((packed)) TcpState {
    uint16_t magic_word=TCP_MAGIC_WORD;
    uint8_t state_num;
    uint8_t mode_num;
    uint8_t Nx=90; 
    uint8_t Ny=90;
    uint8_t tpu;
    uint8_t cam;
    uint8_t sdcard;
    bool operator==(const TcpState& other) const {
        return magic_word == other.magic_word &&
               state_num   == other.state_num &&
               mode_num    == other.mode_num &&
               Nx          == other.Nx &&
               Ny          == other.Ny &&
               tpu         == other.tpu &&
               cam         == other.cam &&
               sdcard      == other.sdcard;
    }
    bool operator!=(const TcpState& other) const {
        return !(*this == other);
    }
};

/**
    Motor info
*/
struct Position {
    uint8_t yaw =90;
    uint8_t pitch = 90;
};
/**

    Target info
*/
struct TargetInfo {
    std::atomic<uint8_t> id;

    int16_t x;
    int16_t y;
    mutable std::mutex mtx;
public:
    void setXY(int16_t new_x, int16_t new_y) {
        std::lock_guard<std::mutex> lock(mtx);
        x = new_x;
        y = new_y;
    }

    std::pair<int16_t, int16_t> getXY() const {
        std::lock_guard<std::mutex> lock(mtx);
        return {x, y};
    }
};

struct SystemInfo {
    std::atomic<State> current_state=State::CHECKING; 
    std::atomic<Mode> current_mode=Mode::MANUAL; 
    std::atomic<bool> TCP_state_connected=false; 
    std::atomic<bool> TCP_cmd_connected=false; 
    bool TPU_state=false; 
    bool CAM_state=false; 
    bool logging_enabled=false; 
    double cpu_temp;
};

struct StateSync {
    std::mutex mtx;
    std::condition_variable cv;
};





extern std::vector<std::string> State_str;
extern std::vector<std::string> Mode_str;

extern StateSync statesync;
extern SystemInfo sysInfo;

extern Cam_opt cam_opt;

extern Position pos;
extern std::mutex pos_mtx;

extern TargetInfo targetInfo;
extern std::unordered_map<int,std::string> m_track_label;

#define INFER_QUEUE_SIZE 1
#define SEND_QUEUE_SIZE 5
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t max_size_;

public:
    explicit ThreadSafeQueue(size_t max_size = 1) : max_size_(max_size) {}
    void push(const T& item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.size()>max_size_)
                queue_.pop();
            queue_.push(item);
        }
        cv_.notify_one();
    }

    // 블로킹 pop
    T wait_and_pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]{ return !queue_.empty(); });
        T item = queue_.front();
        queue_.pop();
        return item;
    }

    // 논블로킹 pop
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = queue_.front();
        queue_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}
};

extern const std::unordered_set<std::string> allowed_labels ;

using FramePtr = std::shared_ptr<FrameData>;
extern ThreadSafeQueue<FramePtr> enhance_to_infer;
extern std::vector<InferenceResult> InferResult;
extern std::mutex infer_mtx;

struct SendPacket {
    FramePtr frame;
    std::vector<ObjectInfo> objects;
};

extern ThreadSafeQueue<SendPacket> send_queue;