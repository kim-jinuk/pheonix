#include "detection_camera.h"

#include "edgetpu.h"
#include "opencv2/opencv.hpp"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"
#include "tracking/byte_tracker.hpp"

#include <chrono>

namespace edge {

DetectionCamera::DetectionCamera(
    const std::string& model_path, const std::string& label_path, const float threshold,
    std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context, const bool edgetpu, const int source,
    const int height, const int width, const bool verbose, const std::vector<std::string>& preprocess)
    : m_interpreter(model_path, label_path, threshold, edgetpu_context, edgetpu),
      m_camera(source),
      m_height(height),
      m_width(width),
      m_verbose(verbose),
      m_preprocess(preprocess) {}

void DetectionCamera::Run() {
  // ───────── ByteTracker 초기화 ─────────────────────────────────────
  tracking::ByteTracker tracker;

  const auto& input_tensor_shape = m_interpreter.GetInputShape();
  const auto width = input_tensor_shape[1];
  const auto height = input_tensor_shape[2];

  // Initializing cameras.
  if (!m_camera.isOpened()) {
    std::cerr << "Unable to open camera!\n";
    exit(0);
  } else {
    m_camera.set(cv::CAP_PROP_FPS, 30.0f);
    m_camera.set(cv::CAP_PROP_FRAME_HEIGHT, m_height);
    m_camera.set(cv::CAP_PROP_FRAME_WIDTH, m_width);
    // In case user gives incorrect parameters, cv will re-adjust, we reset our
    // values to fit cv.
    m_height = m_camera.get(cv::CAP_PROP_FRAME_HEIGHT);
    m_width = m_camera.get(cv::CAP_PROP_FRAME_WIDTH);
  }

  const auto& cvred = cv::Scalar(0, 0, 255);
  const auto& cvblue = cv::Scalar(255, 0, 0);
  cv::Mat frame;
  for (;;) {
    m_camera.read(frame);
    if (!m_camera.read(frame)) break;  // Blank frame!
    ++m_frame_counter;
    cv::Mat resized_frame;
    // Converts image colors.
    cvtColor(frame, resized_frame, cv::COLOR_BGR2RGB);
    // Resize image to fit input tensors shape.
    cv::resize(resized_frame, resized_frame, cv::Size(width, height));
    std::vector<uint8_t> input(
        resized_frame.data,
        resized_frame.data + (resized_frame.cols * resized_frame.rows * resized_frame.elemSize()));

    const auto& candidates = m_interpreter.RunInference(input);
    
    // ───────────── 후보를 ByteTrack 형식으로 변환 ──────────────
    std::vector<tracking::Detection> dets;
    dets.reserve(candidates.size());
    for (const auto& c : candidates) {
      tracking::Detection d;
      d.bbox  = {
          c.x1 * m_width,
          c.y1 * m_height,
          (c.x2 - c.x1) * m_width,
          (c.y2 - c.y1) * m_height};
      d.score = c.score;
      dets.push_back(std::move(d));
    }

    // ① 트래커 업데이트 (예측 + 보정) → (예측박스, id) 목록
    const auto tracks = tracker.update(dets);
    // ② detection ↔ track 매칭 (IoU 기반, 단순 greedy)
    const float IOU_THR = 0.3f;
    auto IoU = [](const cv::Rect2f& a, const cv::Rect2f& b){
        float inter = (a & b).area();
        float uni   = a.area() + b.area() - inter;
        return uni > 0.f ? inter / uni : 0.f;
    };

    std::vector<int> det_ids(dets.size(), -1);          // detection ↔ id 매핑
    std::vector<bool> track_used(tracks.size(), false);

    for (size_t di = 0; di < dets.size(); ++di) {
      float best = IOU_THR; int best_ti = -1;
      for (size_t ti = 0; ti < tracks.size(); ++ti) {
        if (track_used[ti]) continue;
        float iou = IoU(dets[di].bbox, tracks[ti].first);
        if (iou > best) { best = iou; best_ti = static_cast<int>(ti); }
      }
      if (best_ti >= 0) {
        det_ids[di]        = tracks[best_ti].second;  // ID 할당
        track_used[best_ti] = true;
      }
    }
    
    std::cout << "Inference Time: " << m_interpreter.get_prev_duration().count()
              << " microseconds\n";

    const auto& f = "Inference Rate: "
                    + std::to_string(1000000 / m_interpreter.get_prev_duration().count()) + " fps";
    cv::putText(frame, f, cv::Point(0, 20), cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5, 8, 0);

    // ───────────── ID + class + score 오버레이 ───────────────
    for (size_t i = 0; i < dets.size(); ++i) {
      int id = det_ids[i];
      if (id < 0) continue;                  // 매칭 실패한 박스는 건너뜀

      const auto& bb = dets[i].bbox;         // detection 박스 그대로
      const auto& c  = candidates[i];        // class / score
      int lft = static_cast<int>(bb.x + 0.5f);
      int top = static_cast<int>(bb.y + 0.5f);
      int rgt = static_cast<int>(bb.x + bb.width  + 0.5f);
      int btm = static_cast<int>(bb.y + bb.height + 0.5f);

      cv::rectangle(frame, {lft, top}, {rgt, btm}, cvblue, 2, 1, 0);
      std::string tag = "ID:" + std::to_string(id) +
                        "  "   + c.candidate +
                        "  "   + std::to_string(c.score).substr(0,4);
      cv::putText(frame, tag, {lft, top - 5},
                  cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5, 8, 0);

      if (m_verbose) {
        std::cout << "\n-----\nFrame " << m_frame_counter
                  << "  " << tag
                  << "  top:" << top << " lft:" << lft
                  << "  btm:" << btm << " rgt:" << rgt;
      }
    }
 
     cv::imshow("Live Inference", frame);
     cv::waitKey(1);
   }
 }


DetectionCamera::~DetectionCamera() {}

}  // namespace edge