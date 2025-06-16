#pragma once
#include <opencv2/opencv.hpp>

class KalmanTracker {
public:
    KalmanTracker(const cv::Rect& init_box, int id);
    void predict();
    void update(const cv::Rect& meas_box);
    cv::Rect get_box() const;
    int id() const { return id_; }
    int time_since_update() const { return time_since_update_; }
    int hits() const { return hits_; }
    int age() const { return age_; }
    void mark_missed() { ++time_since_update_; }
    void mark_updated() { time_since_update_ = 0; ++hits_; }
    void inc_age() { ++age_; }

private:
    cv::KalmanFilter kf_;
    cv::Mat state_;
    int id_, hits_, age_, time_since_update_;
    static int next_id_;
};