#include "tracking_csc/sort_tracker.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>


namespace tracking {

static cv::KalmanFilter createKF(const cv::Rect2f &bbox) {
    cv::KalmanFilter kf(7, 4, 0);
    // state = [cx, cy, s, r, vx, vy, vs]
    kf.transitionMatrix = (cv::Mat_<float>(7,7) <<
        1,0,0,0,1,0,0,
        0,1,0,0,0,1,0,
        0,0,1,0,0,0,1,
        0,0,0,1,0,0,0,
        0,0,0,0,1,0,0,
        0,0,0,0,0,1,0,
        0,0,0,0,0,0,1);
    setIdentity(kf.measurementMatrix);
    setIdentity(kf.processNoiseCov, cv::Scalar::all(1e-2));
    setIdentity(kf.measurementNoiseCov, cv::Scalar::all(1e-1));
    setIdentity(kf.errorCovPost, cv::Scalar::all(1));
    float cx = bbox.x + bbox.width/2;
    float cy = bbox.y + bbox.height/2;
    float s  = bbox.area();
    float r  = bbox.width / bbox.height;
    kf.statePost = (cv::Mat_<float>(7,1) << cx,cy,s,r,0,0,0);
    return kf;
}

SortTracker::SortTracker(float iou) : iou_thresh_(iou) {}

std::vector<std::pair<cv::Rect2f,int>> SortTracker::predict_only()
{
    for (auto &t : tracks_) step_kalman(t);        // 칼만 예측만
    std::vector<std::pair<cv::Rect2f,int>> out;
    for (auto &t : tracks_) out.emplace_back(t.bbox, t.id);
    return out;
}

float SortTracker::IoU(const cv::Rect2f &a, const cv::Rect2f &b) {
    const float inter = (a & b).area();
    const float uni = a.area() + b.area() - inter;
    return uni > 0 ? inter/uni : 0.f;
}

void SortTracker::step_kalman(Track &t) {
    cv::Mat pred = t.kf.predict();
    float cx = pred.at<float>(0), cy = pred.at<float>(1);
    float s = pred.at<float>(2), r = pred.at<float>(3);
    float w = std::sqrt(s*r);
    float h = s / w;
    t.bbox = {cx - w/2, cy - h/2, w, h};
}

std::vector<std::pair<cv::Rect2f,int>> SortTracker::update(const std::vector<cv::Rect2f>& dets) {
    // 1. Predict existing tracks
    for (auto &t : tracks_) step_kalman(t);

    // 2. Associate detections ↔ tracks via IoU (greedy)
    std::vector<int> det_matched(dets.size(), -1);
    for (size_t ti = 0; ti < tracks_.size(); ++ti) {
        float best_iou = iou_thresh_;
        int best_di = -1;
        for (size_t di = 0; di < dets.size(); ++di) {
            if (det_matched[di] != -1) continue;
            float iou = IoU(tracks_[ti].bbox, dets[di]);
            if (iou > best_iou) { best_iou = iou; best_di = di; }
        }
        if (best_di >= 0) {
            // measurement update
            const cv::Rect2f &bbox = dets[best_di];
            float cx = bbox.x + bbox.width/2;
            float cy = bbox.y + bbox.height/2;
            float s  = bbox.area();
            float r  = bbox.width / bbox.height;
            tracks_[ti].kf.correct((cv::Mat_<float>(4,1)<<cx,cy,s,r));
            tracks_[ti].bbox = bbox;
            tracks_[ti].time_since_update = 0;
            det_matched[best_di] = ti;
        } else {
            tracks_[ti].time_since_update++;
        }
    }

    // 3. Spawn new tracks for unmatched detections
    for (size_t di = 0; di < dets.size(); ++di) if (det_matched[di]==-1) {
        Track t;
        t.id = next_id_++;
        t.kf = createKF(dets[di]);
        t.bbox = dets[di];
        tracks_.push_back(std::move(t));
    }

    // 4. Cull stale tracks (no update for >30 frames)
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(), [](const Track &t){return t.time_since_update>10;}), tracks_.end());

    // 5. Output list
    std::vector<std::pair<cv::Rect2f,int>> out;
    for (auto &t : tracks_) out.emplace_back(t.bbox, t.id);
    return out;
}

} // namespace tracking