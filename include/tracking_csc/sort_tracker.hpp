#pragma once
#include <opencv2/opencv.hpp>
#include "tracking/kalman_tracker.hpp"
#include <vector>
#include <deque>
#include <algorithm>

struct TrackResult {
    int id;
    cv::Rect box;
};

class SortTracker {
public:
    SortTracker(int max_age=10, int min_hits=3, float iou_thr=0.3);
    std::vector<TrackResult> update(const std::vector<cv::Rect>& dets);

private:
    float iou(const cv::Rect& a, const cv::Rect& b);
    int max_age_, min_hits_, next_id_;
    float iou_thr_;
    std::vector<KalmanTracker> trackers_;
};