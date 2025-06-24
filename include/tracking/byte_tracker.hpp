// byte_tracker.hpp – minimal ByteTrack header‑only tracker (Kalman + 2‑stage IoU)
// Re‑writes v2 – fixes GCC < 9 errors & removes default‑init confusion
// Author: ChatGPT 2025‑06‑22

#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

namespace tracking {

//------------------------------------------------------------------
// Detection input (bbox + confidence)
//------------------------------------------------------------------
struct Detection {
    cv::Rect2f bbox;
    float      score;
};

//------------------------------------------------------------------
// Track state
//------------------------------------------------------------------
struct Track {
    int id = -1;
    cv::KalmanFilter kf;
    cv::Rect2f bbox;
    int time_since_update = 0;
};

//------------------------------------------------------------------
class ByteTracker {
public:
    //------------------------------------------------------------------
    // Parameter block (no in‑class init to keep pre‑C++14 compilers happy)
    //------------------------------------------------------------------
    struct Params {
        float high_thr;
        float low_thr;
        float iou_thr_high;
        float iou_thr_low;
        int   max_age;
        Params() :
            high_thr(0.6f), low_thr(0.3f),
            iou_thr_high(0.3f), iou_thr_low(0.2f),
            max_age(30) {}
    } p;

    explicit ByteTracker(const Params& prm = Params{}) : p(prm) {}

    // update – call every frame (detections may be empty)
    std::vector<std::pair<cv::Rect2f,int>> update(const std::vector<Detection>& dets){
        //------ split detections by confidence ---------------------------
        std::vector<Detection> hi, lo;
        for(const auto& d:dets){
            if(d.score >= p.high_thr) hi.push_back(d);
            else if(d.score >= p.low_thr) lo.push_back(d);
        }

        //------ 1. predict all tracks ------------------------------------
        for(auto& t:tracks_) predict(t);

        detected_.assign(tracks_.size(), false);
        std::vector<int> match_hi(hi.size(), -1);
        std::vector<int> match_lo(lo.size(), -1);

        //------ 2‑A stage‑1 high‑conf match ------------------------------
        greedy_match(hi, p.iou_thr_high, match_hi);
        //------ 2‑B stage‑2 low‑conf match -------------------------------
        greedy_match(lo, p.iou_thr_low, match_lo);

        //------ 3. spawn tracks for unmatched hi‑conf --------------------
        for(size_t i=0;i<hi.size();++i) if(match_hi[i]==-1) spawn(hi[i]);

        //------ 4. age & cull -------------------------------------------
        tracks_.erase(std::remove_if(tracks_.begin(),tracks_.end(),
            [&](const Track& t){return t.time_since_update>p.max_age;}), tracks_.end());

        //------ 5. pack result ------------------------------------------
        std::vector<std::pair<cv::Rect2f,int>> out; out.reserve(tracks_.size());
        for(auto& t:tracks_) out.emplace_back(t.bbox,t.id);
        return out;
    }

    std::vector<int> assign_ids(const std::vector<Detection>& dets) {
        // 0) 결과 벡터 초기화 (-1: 미매칭)
        std::vector<int> id_out(dets.size(), -1);

        // 1) 예측 단계 (track 내부 bbox 갱신)
        for (auto& t : tracks_) predict(t);

        detected_.assign(tracks_.size(), false);

        // 2) 그리디 매칭 (high / low 구분 없이 한 번에)
        for (size_t di = 0; di < dets.size(); ++di) {
            float best_iou = p.iou_thr_high;   // high_thr 기준
            int   best_ti  = -1;
            for (size_t ti = 0; ti < tracks_.size(); ++ti) {
                if (detected_[ti]) continue;
                float iou = IoU(tracks_[ti].bbox, dets[di].bbox);
                if (iou > best_iou) { best_iou = iou; best_ti = static_cast<int>(ti); }
            }
            if (best_ti >= 0) {
                // 실측으로 칼만 보정(하지만 bbox는 유지)
                correct(tracks_[best_ti], dets[di].bbox);
                detected_[best_ti] = true;
                id_out[di]        = tracks_[best_ti].id;
            }
        }

        // 3) hi-conf (score ≥ high_thr) 중 미매칭 → 새 트랙 생성
        for (size_t di = 0; di < dets.size(); ++di) {
            if (id_out[di] != -1) continue;            // 이미 매칭
            if (dets[di].score < p.high_thr) continue; // hi-conf 아님
            spawn(dets[di]);                           // 새 트랙 생성
            id_out[di] = tracks_.back().id;            // 방금 만든 ID 기록
        }

        // 4) age & cull
        for (auto& t : tracks_) t.time_since_update++;
        tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                    [&](const Track& t) { return t.time_since_update > p.max_age; }),
                    tracks_.end());

        return id_out;
    }

private:
    //------------------------------------------------------------------
    static float IoU(const cv::Rect2f&a,const cv::Rect2f&b){
        float inter=(a&b).area(); float uni=a.area()+b.area()-inter;
        return uni>0? inter/uni:0.f;
    }

    static cv::KalmanFilter create_kf(const cv::Rect2f &bb){
        cv::KalmanFilter kf(7,4,0);
        kf.transitionMatrix=(cv::Mat_<float>(7,7)<<
            1,0,0,0,1,0,0,
            0,1,0,0,0,1,0,
            0,0,1,0,0,0,1,
            0,0,0,1,0,0,0,
            0,0,0,0,1,0,0,
            0,0,0,0,0,1,0,
            0,0,0,0,0,0,1);
        setIdentity(kf.measurementMatrix);
        setIdentity(kf.processNoiseCov,cv::Scalar::all(1e-2));
        setIdentity(kf.measurementNoiseCov,cv::Scalar::all(1e-1));
        setIdentity(kf.errorCovPost,cv::Scalar::all(1));
        float cx=bb.x+bb.width/2, cy=bb.y+bb.height/2;
        float s=bb.area(), r=bb.width/bb.height;
        kf.statePost=(cv::Mat_<float>(7,1)<<cx,cy,s,r,0,0,0);
        return kf;
    }

    void predict(Track &t){
        cv::Mat pred=t.kf.predict();
        float cx=pred.at<float>(0), cy=pred.at<float>(1);
        float s=pred.at<float>(2), r=pred.at<float>(3);
        float w=std::sqrt(s*r), h=s/w;
        t.bbox={cx-w/2, cy-h/2, w, h};
        t.time_since_update++;
    }

    void greedy_match(const std::vector<Detection>& dets, float thr, std::vector<int>& det_match){
        for(size_t ti=0; ti<tracks_.size(); ++ti){
            if(detected_[ti]) continue;
            float best_iou=thr; int best_di=-1;
            for(size_t di=0; di<dets.size(); ++di){
                if(det_match[di]!=-1) continue;
                float iou=IoU(tracks_[ti].bbox,dets[di].bbox);
                if(iou>best_iou){best_iou=iou; best_di=di;}
            }
            if(best_di>=0){
                correct(tracks_[ti], dets[best_di].bbox);
                det_match[best_di]=static_cast<int>(ti);
                detected_[ti]=true;
            }
        }
    }

    void correct(Track &t,const cv::Rect2f &bb){
        float cx=bb.x+bb.width/2, cy=bb.y+bb.height/2;
        float s=bb.area(), r=bb.width/bb.height;
        t.kf.correct((cv::Mat_<float>(4,1)<<cx,cy,s,r));
        t.bbox=bb; t.time_since_update=0;
    }

    void spawn(const Detection& d){
        Track t; t.id=next_id_++; t.kf=create_kf(d.bbox); t.bbox=d.bbox; t.time_since_update=0;
        tracks_.push_back(std::move(t)); detected_.push_back(false);
    }

    //------------------------------------------------------------------
    int next_id_ = 0;
    std::vector<Track> tracks_;
    std::vector<bool>  detected_; // temp flag per frame
};

} // namespace tracking
