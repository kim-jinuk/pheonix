#include "tracking_csc/sort_tracker.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>

#if TRACKING_VERSION == 1
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
// add
     // ── 1) 칼만 예측 + 미매칭 카운트 증가
    for (auto &t : tracks_) {
        step_kalman(t);   // 위치만 예측
        t.time_since_update++;          // ← 추가
    }

    // ── 2) (선택) 오래된 트랙 정리  ───────────────────
    const int STALE_THRESH = 30;        // SKIP=2일 때 30×(2+1)=90
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                    [&](const Track &tr){ return tr.time_since_update > STALE_THRESH; }),
                  tracks_.end());
    // ────────────────────────────────────────────────

// end
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
            //  매칭 실패 출력 
          /*
            const auto& box = tracks_[ti].bbox;
            std::cout << "[UNMATCHED] Track ID " << tracks_[ti].id
                    << " bbox=(" << box.x << "," << box.y << "," << box.width << "," << box.height << ")\n";

            for (size_t di = 0; di < dets.size(); ++di) {
                float iou = IoU(tracks_[ti].bbox, dets[di]);
                std::cout << "    IoU with detection[" << di << "] = "
                        << iou << " → "
                        << (iou < iou_thresh_ ? "too low" : "ok") << "\n";
            }

            std::cout << "    → No match: all IoU < threshold (" << iou_thresh_ << ")\n";
        */
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

#elif TRACKING_VERSION == 2
namespace tracking {

ByteTracker::Params::Params()
    : high_thr(0.6f), low_thr(0.3f),
      iou_thr_high(0.3f), iou_thr_low(0.2f),
       max_age(30)
      {}

ByteTracker::ByteTracker(const Params& prm) : p(prm) {}

std::vector<std::pair<cv::Rect2f,int>> ByteTracker::update(const std::vector<Detection>& dets){
    std::vector<Detection> hi, lo;
    for(const auto& d : dets){
        if(d.score >= p.high_thr) hi.push_back(d);
        else if(d.score >= p.low_thr) lo.push_back(d);
    }

    for(auto& t : tracks_) predict(t);

    detected_.assign(tracks_.size(), false);
    std::vector<int> match_hi(hi.size(), -1);
    std::vector<int> match_lo(lo.size(), -1);

    greedy_match(hi, p.iou_thr_high, match_hi);
    greedy_match(lo, p.iou_thr_low, match_lo);

    for(size_t i = 0; i < hi.size(); ++i)
        if(match_hi[i] == -1) spawn(hi[i]);

    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
        [&](const Track& t){ return t.time_since_update > p.max_age; }), tracks_.end());

    std::vector<std::pair<cv::Rect2f,int>> out;
    out.reserve(tracks_.size());
    for(auto& t : tracks_) out.emplace_back(t.bbox, t.id);
    return out;
}
std::vector<int> ByteTracker::assign_ids(const std::vector<Detection>& dets) {
    std::vector<int> id_out(dets.size(), -1);

    for (auto& t : tracks_) predict(t);
    detected_.assign(tracks_.size(), false);

    for (size_t di = 0; di < dets.size(); ++di) {
        float best_iou = p.iou_thr_high;
        int best_ti = -1;
        for (size_t ti = 0; ti < tracks_.size(); ++ti) {
            if (detected_[ti]) continue;
            float iou = IoU(tracks_[ti].bbox, dets[di].bbox);
            if (iou > best_iou) { best_iou = iou; best_ti = static_cast<int>(ti); }
        }
        if (best_ti >= 0) {
            correct(tracks_[best_ti], dets[di].bbox);
            detected_[best_ti] = true;
            id_out[di] = tracks_[best_ti].id;
        }
    }

    for (size_t di = 0; di < dets.size(); ++di) {
        if (id_out[di] != -1) continue;
        if (dets[di].score < p.high_thr) continue;
        spawn(dets[di]);
        id_out[di] = tracks_.back().id;
    }

    for (auto& t : tracks_) t.time_since_update++;
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
        [&](const Track& t) { return t.time_since_update > p.max_age; }),
        tracks_.end());

    return id_out;
}

float ByteTracker::IoU(const cv::Rect2f& a, const cv::Rect2f& b){
    float inter = (a & b).area();
    float uni = a.area() + b.area() - inter;
    return uni > 0 ? inter / uni : 0.f;
}

cv::KalmanFilter ByteTracker::create_kf(const cv::Rect2f &bb){
    cv::KalmanFilter kf(7, 4, 0);
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
    float cx = bb.x + bb.width / 2;
    float cy = bb.y + bb.height / 2;
    float s = bb.area();
    float r = bb.width / bb.height;
    kf.statePost = (cv::Mat_<float>(7,1) << cx, cy, s, r, 0, 0, 0);
    return kf;
}

void ByteTracker::predict(Track &t){
    cv::Mat pred = t.kf.predict();
    float cx = pred.at<float>(0), cy = pred.at<float>(1);
    float s = pred.at<float>(2), r = pred.at<float>(3);
    float w = std::sqrt(s * r), h = s / w;
    t.bbox = {cx - w / 2, cy - h / 2, w, h};
    t.time_since_update++;
}

void ByteTracker::greedy_match(const std::vector<Detection>& dets, float thr, std::vector<int>& det_match){
    for(size_t ti = 0; ti < tracks_.size(); ++ti){
        if(detected_[ti]) continue;
        float best_iou = thr;
        int best_di = -1;
        for(size_t di = 0; di < dets.size(); ++di){
            if(det_match[di] != -1) continue;
            float iou = IoU(tracks_[ti].bbox, dets[di].bbox);
            if(iou > best_iou){
                best_iou = iou;
                best_di = di;
            }
        }
        if(best_di >= 0){
            correct(tracks_[ti], dets[best_di].bbox);
            det_match[best_di] = static_cast<int>(ti);
            detected_[ti] = true;
        }
    }
}

void ByteTracker::correct(Track &t, const cv::Rect2f &bb){
    float cx = bb.x + bb.width / 2;
    float cy = bb.y + bb.height / 2;
    float s = bb.area();
    float r = bb.width / bb.height;
    t.kf.correct((cv::Mat_<float>(4,1) << cx, cy, s, r));
    t.bbox = bb;
    t.time_since_update = 0;
}

void ByteTracker::spawn(const Detection& d){
    Track t;
    next_id_++;
    t.id = next_id_%255;
    t.kf = create_kf(d.bbox);
    t.bbox = d.bbox;
    t.time_since_update = 0;
    tracks_.push_back(std::move(t));
    detected_.push_back(false);
}

void ByteTracker::init_id(){
    next_id_=0;
}

} // namespace tracking

#elif TRACKING_VERSION == 3

namespace tracking {

ByteTracker::ByteTracker(const Params& prm) : p(prm) {}

void ByteTracker::setCameraMotion(const cv::Point2f& delta) {
    cam_shift_ = delta;
}

std::vector<std::pair<cv::Rect2f,int>> ByteTracker::update(const std::vector<Detection>& dets) {
    std::vector<Detection> hi, lo;
    for(const auto& d : dets) {
        if(d.score >= p.high_thr) hi.push_back(d);
        else if(d.score >= p.low_thr) lo.push_back(d);
    }

    for(auto &t : tracks_) predict(t);

    detected_.assign(tracks_.size(), false);
    std::vector<int> match_hi(hi.size(), -1), match_lo(lo.size(), -1);

    greedy_match(hi, p.iou_thr_high, match_hi);
    greedy_match(lo, p.iou_thr_low,  match_lo);

    for(size_t i = 0; i < hi.size(); ++i)
        if(match_hi[i] == -1) spawn(hi[i]);

    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
        [&](const Track& t){ return t.time_since_update > p.max_age; }), tracks_.end());

    std::vector<std::pair<cv::Rect2f,int>> out;
    out.reserve(tracks_.size());
    for(auto& t : tracks_) out.emplace_back(t.bbox, t.id);
    return out;
}

float ByteTracker::IoU(const cv::Rect2f& a, const cv::Rect2f& b) {
    float inter = (a & b).area();
    float uni   = a.area() + b.area() - inter;
    return uni > 0 ? inter / uni : 0.f;
}

cv::KalmanFilter ByteTracker::create_kf(const cv::Rect2f &bb) {
    cv::KalmanFilter kf(7, 4, 0);
    kf.transitionMatrix = (cv::Mat_<float>(7,7) <<
        1,0,0,0,1,0,0,
        0,1,0,0,0,1,0,
        0,0,1,0,0,0,1,
        0,0,0,1,0,0,0,
        0,0,0,0,1,0,0,
        0,0,0,0,0,1,0,
        0,0,0,0,0,0,1);
    setIdentity(kf.measurementMatrix);
    setIdentity(kf.processNoiseCov,      cv::Scalar::all(p.q_scale));
    setIdentity(kf.measurementNoiseCov,  cv::Scalar::all(p.r_scale));
    setIdentity(kf.errorCovPost,         cv::Scalar::all(1));

    float cx = bb.x + bb.width / 2;
    float cy = bb.y + bb.height / 2;
    float s  = bb.area();
    float r  = bb.width / bb.height;
    kf.statePost = (cv::Mat_<float>(7,1) << cx, cy, s, r, 0, 0, 0);
    return kf;
}

void ByteTracker::predict(Track &t) {
    cv::Mat pred = t.kf.predict();
    float cx = pred.at<float>(0), cy = pred.at<float>(1);
    float s  = pred.at<float>(2), r  = pred.at<float>(3);
    float w  = std::sqrt(std::max(s*r, 1e-6f));
    float h  = s / std::max(w, 1e-6f);

    cx -= cam_shift_.x;
    cy -= cam_shift_.y;

    t.bbox = {cx - w/2, cy - h/2, w, h};
    t.time_since_update++;
}

void ByteTracker::greedy_match(const std::vector<Detection>& dets, float thr, std::vector<int>& dmatch) {
    for(size_t ti = 0; ti < tracks_.size(); ++ti) {
        if(detected_[ti]) continue;
        float best = thr;
        int best_di = -1;

        for(size_t di = 0; di < dets.size(); ++di) {
            if(dmatch[di] != -1) continue;
            float iou = IoU(tracks_[ti].bbox, dets[di].bbox);
            if(iou > best) {
                best = iou;
                best_di = static_cast<int>(di);
            }
        }

        if(best_di >= 0) {
            correct(tracks_[ti], dets[best_di].bbox);
            dmatch[best_di] = static_cast<int>(ti);
            detected_[ti] = true;
        }
    }
}

void ByteTracker::correct(Track &t, const cv::Rect2f &bb) {
    float cx = bb.x + bb.width / 2;
    float cy = bb.y + bb.height / 2;
    float s  = bb.area();
    float r  = bb.width / bb.height;

    t.kf.correct((cv::Mat_<float>(4,1) << cx, cy, s, r));
    t.bbox = bb;
    t.time_since_update = 0;
}

void ByteTracker::spawn(const Detection& d) {
    Track t;
    t.id = next_id_++;
    t.kf = create_kf(d.bbox);
    t.bbox = d.bbox;
    t.time_since_update = 0;

    tracks_.push_back(std::move(t));
    detected_.push_back(false);
}

} 

#endif