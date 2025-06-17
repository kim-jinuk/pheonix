#include "tracking_csc/sort_tracker.hpp"




SortTracker::SortTracker(int max_age, int min_hits, float iou_thr)
    : max_age_(max_age), min_hits_(min_hits), iou_thr_(iou_thr), next_id_(1) {}

float SortTracker::iou(const cv::Rect& a, const cv::Rect& b) {
    int x1 = std::max(a.x, b.x), y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x+a.width, b.x+b.width), y2 = std::min(a.y+a.height, b.y+b.height);
    int inter = std::max(0, x2-x1) * std::max(0, y2-y1);
    int union_ = a.area() + b.area() - inter;
    return union_ == 0 ? 0.0f : float(inter)/union_;
}

std::vector<TrackResult> SortTracker::update(const std::vector<cv::Rect>& dets) {
    // 1. 트랙 예측
    for (auto& t : trackers_) t.predict();

    // 2. IoU 비용행렬 생성 (track x detection)
    std::vector<std::vector<float>> cost_matrix(trackers_.size(), std::vector<float>(dets.size(), 1.0f));
    for (size_t i = 0; i < trackers_.size(); ++i)
        for (size_t j = 0; j < dets.size(); ++j)
            cost_matrix[i][j] = 1.0f - iou(trackers_[i].get_box(), dets[j]); // 1-IoU

    // 3. Hungarian 할당
    std::vector<int> assignment;
    Hungarian::solve(cost_matrix, assignment);

    std::vector<bool> det_used(dets.size(), false);
    for (size_t i = 0; i < trackers_.size(); ++i) {
        if (assignment[i] >= 0 && 1.0f - cost_matrix[i][assignment[i]] > iou_thr_) {
            trackers_[i].update(dets[assignment[i]]);
            det_used[assignment[i]] = true;
        }
    }

    // 4. 미할당 디텍션은 새 트랙 생성
    for (size_t j = 0; j < dets.size(); ++j) {
        if (!det_used[j]) {
            trackers_.emplace_back(dets[j], next_id_++);
        }
    }

    // 5. 트랙 관리: 삭제, 나이/갱신 등
    std::vector<KalmanTracker> new_trackers;
    std::vector<TrackResult> results;
    for (auto& t : trackers_) {
        if (t.time_since_update() < max_age_) {
            if (t.hits() >= min_hits_ || t.age() <= min_hits_)
                results.push_back({t.id(), t.get_box()});
            new_trackers.push_back(t);
        }
    }
    trackers_ = new_trackers;
    return results;
}