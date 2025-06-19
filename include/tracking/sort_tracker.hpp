#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

namespace tracking {

struct Track {
    int id;
    cv::Rect2f bbox;
    float vx = 0.f, vy = 0.f;   // velocity (dead‑reckoning)
    int miss = 0;
};

class SortTracker {
 public:
  explicit SortTracker(float iou_thr = 0.3f);

  /**
   * @brief Update tracker with current detections
   * @param detections bounding boxes in norm/absolute coords
   * @return vector of <bbox,id>
   */
  std::vector<std::pair<cv::Rect2f,int>> update(const std::vector<cv::Rect2f>& detections);
  float dist2(const cv::Rect2f& a, const cv::Rect2f& b) const;
  static constexpr int kMaxTracks = 32;

 private:
  float IoU(const cv::Rect2f& a, const cv::Rect2f& b) const;

  float iou_thr_;
  int next_id_ = 0;
  std::vector<Track> tracks_;
};

} // namespace tracking