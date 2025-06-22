#pragma once
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>

#define TRACKING_VERSION 2

#if TRACKING_VERSION == 1
namespace tracking {

class SortTracker {
 public:
    explicit SortTracker(float iou_threshold = 0.3f);
    // Update with freshly detected boxes; returns (bbox, id)
    std::vector<std::pair<cv::Rect2f,int>> update(const std::vector<cv::Rect2f>& detections);
    std::vector<std::pair<cv::Rect2f,int>> predict_only();
 private:
    float iou_thresh_;
    int next_id_ = 0;
    std::vector<Track> tracks_;
    static float IoU(const cv::Rect2f &a, const cv::Rect2f &b);
    void step_kalman(Track &t);
};
}
#elif TRACKING_VERSION == 2
namespace tracking {

// Detection input (bbox + confidence)
struct Detection {
    cv::Rect2f bbox;
    float      score;
};

// Track state
struct Track {
    int id = -1;
    cv::KalmanFilter kf;
    cv::Rect2f bbox;
    int time_since_update = 0;
}; 

class ByteTracker {
public:
    struct Params {
        float high_thr;
        float low_thr;
        float iou_thr_high;
        float iou_thr_low;
        int   max_age;
        Params();
    } p;

    explicit ByteTracker(const Params& prm = Params{});

    std::vector<std::pair<cv::Rect2f,int>> update(const std::vector<Detection>& dets);

private:
    static float IoU(const cv::Rect2f&a,const cv::Rect2f&b);
    static cv::KalmanFilter create_kf(const cv::Rect2f &bb);

    void predict(Track &t);
    void greedy_match(const std::vector<Detection>& dets, float thr, std::vector<int>& det_match);
    void correct(Track &t,const cv::Rect2f &bb);
    void spawn(const Detection& d);

    int next_id_ = 0;
    std::vector<Track> tracks_;
    std::vector<bool>  detected_;
};

} // namespace tracking

#elif TRACKING_VERSION == 3
namespace tracking {

//--------------------------------------------------
// Detection input (bbox + confidence)
//--------------------------------------------------
struct Detection {
    cv::Rect2f bbox;
    float      score;
};

//--------------------------------------------------
// Internal track state
//--------------------------------------------------
struct Track {
    int id = -1;
    cv::KalmanFilter kf;
    cv::Rect2f bbox;
    int time_since_update = 0;
};

//--------------------------------------------------
// ByteTracker class
//--------------------------------------------------
class ByteTracker {
public:
    struct Params {
        float high_thr      = 0.6f;
        float low_thr       = 0.3f;
        float iou_thr_high  = 0.3f;
        float iou_thr_low   = 0.2f;
        int   max_age       = 60;
        float q_scale       = 1e-1f;
        float r_scale       = 1e-1f;
    } p;

    ByteTracker() = default;
    explicit ByteTracker(const Params& prm);

    void setCameraMotion(const cv::Point2f& delta);

    std::vector<std::pair<cv::Rect2f,int>> update(const std::vector<Detection>& dets);

private:
    static float IoU(const cv::Rect2f& a, const cv::Rect2f& b);
    cv::KalmanFilter create_kf(const cv::Rect2f& bb);

    void predict(Track &t);
    void greedy_match(const std::vector<Detection>& dets, float thr, std::vector<int>& dmatch);
    void correct(Track &t, const cv::Rect2f &bb);
    void spawn(const Detection& d);

    int next_id_ = 0;
    std::vector<Track> tracks_;
    std::vector<bool>  detected_;
    cv::Point2f        cam_shift_{0,0};
};

} // namespace tracking
#endif