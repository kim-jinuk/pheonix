#include "tracking/sort_tracker.hpp"
#include <algorithm>

namespace tracking {

SortTracker::SortTracker(float iou_thr) : iou_thr_(iou_thr) {}

float SortTracker::IoU(const cv::Rect2f& a, const cv::Rect2f& b) const {
    float inter = (a & b).area();
    float uni = a.area() + b.area() - inter;
    return uni > 0 ? inter / uni : 0.f;
}

float SortTracker::dist2(const cv::Rect2f& a, const cv::Rect2f& b) const {
    float ax = a.x + a.width*0.5f, ay = a.y + a.height*0.5f;
    float bx = b.x + b.width*0.5f, by = b.y + b.height*0.5f;
    float dx = ax - bx, dy = ay - by;
    return dx*dx + dy*dy;
}

std::vector<std::pair<cv::Rect2f,int>> SortTracker::update(const std::vector<cv::Rect2f>& dets) {
    // 1. dead‑reckoning predict
    for (auto &t : tracks_) {
        t.bbox.x += t.vx;
        t.bbox.y += t.vy;
        t.miss++;
    }

    // 2. Greedy IoU match
    std::vector<int> det_used(dets.size(), -1);
    for (auto &t : tracks_) {
        float best = iou_thr_; int best_di = -1;
        for (size_t di=0; di<dets.size(); ++di) if(det_used[di]==-1) {
            float d2 = dist2(t.bbox, dets[di]);
            if (d2 < best) { best = d2; best_di = static_cast<int>(di); }
        }
        if (best_di >= 0 && IoU(t.bbox, dets[best_di]) >= iou_thr_) {
            const auto &d = dets[best_di];
            // velocity EMA (α=0.7)
            t.vx = 0.7f * (d.x - t.bbox.x) + 0.3f * t.vx;
            t.vy = 0.7f * (d.y - t.bbox.y) + 0.3f * t.vy;
            t.bbox = d;
            t.miss = 0;
            det_used[best_di] = 1;
        }
    }

    // 3. New tracks
    for (size_t di=0; di<dets.size(); ++di) if(det_used[di]==-1) {
        if (static_cast<int>(tracks_.size()) >= kMaxTracks) break;
        Track t{next_id_++, dets[di]};
        tracks_.push_back(t);
    }

    // 4. Prune stale
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                 [](const Track &t){return t.miss>30;}), tracks_.end());

    // 5. Output
    std::vector<std::pair<cv::Rect2f,int>> out;
    out.reserve(tracks_.size());
    for (auto &t : tracks_) out.emplace_back(t.bbox, t.id);
    return out;
}

} // namespace tracking