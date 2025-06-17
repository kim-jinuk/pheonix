#pragma once
#include "running_control_csc/globals.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>


namespace tracking {

class SortTracker {
 public:
    explicit SortTracker(float iou_threshold = 0.3f);
    // Update with freshly detected boxes; returns (bbox, id)
    std::vector<std::pair<cv::Rect2f,int>> update(const std::vector<cv::Rect2f>& detections);
 private:
    float iou_thresh_;
    int next_id_ = 0;
    std::vector<Track> tracks_;
    static float IoU(const cv::Rect2f &a, const cv::Rect2f &b);
    void step_kalman(Track &t);
};
}