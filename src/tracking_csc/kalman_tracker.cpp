#include "tracking/kalman_tracker.hpp"
int KalmanTracker::next_id_ = 1;

KalmanTracker::KalmanTracker(const cv::Rect& init_box, int id)
    : id_(id ? id : next_id_++), hits_(1), age_(1), time_since_update_(0) {
    // [x, y, s, r, vx, vy, vs]  (s: area, r: ratio)
    kf_ = cv::KalmanFilter(7, 4, 0);
    state_ = cv::Mat::zeros(7, 1, CV_32F);

    float cx = init_box.x + init_box.width / 2.0f;
    float cy = init_box.y + init_box.height / 2.0f;
    float s = init_box.width * init_box.height;
    float r = float(init_box.width) / float(init_box.height);

    state_.at<float>(0) = cx;
    state_.at<float>(1) = cy;
    state_.at<float>(2) = s;
    state_.at<float>(3) = r;

    // Init transition matrix
    kf_.transitionMatrix = (cv::Mat_<float>(7, 7) <<
        1,0,0,0,1,0,0,
        0,1,0,0,0,1,0,
        0,0,1,0,0,0,1,
        0,0,0,1,0,0,0,
        0,0,0,0,1,0,0,
        0,0,0,0,0,1,0,
        0,0,0,0,0,0,1);

    cv::setIdentity(kf_.measurementMatrix);
    cv::setIdentity(kf_.processNoiseCov, cv::Scalar::all(1e-2));
    cv::setIdentity(kf_.measurementNoiseCov, cv::Scalar::all(1e-1));
    cv::setIdentity(kf_.errorCovPost, cv::Scalar::all(1));

    kf_.statePost = state_;
}

void KalmanTracker::predict() {
    state_ = kf_.predict();
    inc_age();
    mark_missed();
}

void KalmanTracker::update(const cv::Rect& meas_box) {
    cv::Mat meas(4,1,CV_32F);
    float cx = meas_box.x + meas_box.width/2.0f;
    float cy = meas_box.y + meas_box.height/2.0f;
    float s  = meas_box.width * meas_box.height;
    float r  = float(meas_box.width) / float(meas_box.height);
    meas.at<float>(0) = cx;
    meas.at<float>(1) = cy;
    meas.at<float>(2) = s;
    meas.at<float>(3) = r;
    kf_.correct(meas);
    state_ = kf_.statePost;
    mark_updated();
}

cv::Rect KalmanTracker::get_box() const {
    float cx = state_.at<float>(0);
    float cy = state_.at<float>(1);
    float s = state_.at<float>(2);
    float r = state_.at<float>(3);
    float w = std::sqrt(s * r);
    float h = s / w;
    return cv::Rect(int(cx - w/2), int(cy - h/2), int(w), int(h));
}