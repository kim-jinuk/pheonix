#include "detection_camera.h"

#include "edgetpu.h"
#include "opencv2/opencv.hpp"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"

#include "preprocessing/edge_enhance.hpp"
#include "tracking/sort_tracker.hpp"

#include <chrono>

namespace edge {

DetectionCamera::DetectionCamera(
    const std::string& model_path, const std::string& label_path, const float threshold,
    std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context, const bool edgetpu, const int source,
    const int height, const int width, const bool verbose)
    : m_interpreter(model_path, label_path, threshold, edgetpu_context, edgetpu),
      m_camera(source),
      m_height(height),
      m_width(width),
      m_verbose(verbose) {}

void DetectionCamera::Run() {
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
  auto start_ts = std::chrono::steady_clock::now();
  for (;;) {
    m_camera.read(frame);
    if (!m_camera.read(frame)) break;  // Blank frame!
    ++m_frame_counter;
    cv::Mat resized_frame;
    cv::Mat enhanced = preprocessing::apply(frame);
    // Converts image colors.
    cvtColor(enhanced, resized_frame, cv::COLOR_BGR2RGB);
    // Resize image to fit input tensors shape.
    cv::resize(resized_frame, resized_frame, cv::Size(width, height));
    std::vector<uint8_t> input(
        resized_frame.data,
        resized_frame.data + (resized_frame.cols * resized_frame.rows * resized_frame.elemSize()));

    const auto& candidates = m_interpreter.RunInference(input);

    std::cout << "Inference Time: " << m_interpreter.get_prev_duration().count()
              << " microseconds\n";
    
    std::vector<cv::Rect2f> det_boxes;
    det_boxes.reserve(candidates.size());
    for (const auto& c : candidates) {
      det_boxes.emplace_back(c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);
    }
    const auto tracked = m_tracker.update(det_boxes);
    auto iou = [](const cv::Rect2f& a, const cv::Rect2f& b){
      float inter = (a & b).area();
      float uni   = a.area() + b.area() - inter;
      return uni>0 ? inter/uni : 0.f;
    };
    for (size_t i = 0; i < tracked.size(); ++i) {
      int id = tracked[i].second;
      // detection → track 매칭 (가장 IoU 큰 박스)
      float best = 0.f;
      std::string best_label;
      for (const auto& c : candidates) {
        cv::Rect2f r(c.x1,c.y1,c.x2-c.x1,c.y2-c.y1);
        float     v = iou(r, tracked[i].first);
        if (v > best) { best = v; best_label = c.candidate; }
      }
      if (best > 0.3f) m_track_label[id] = best_label;
    }
    
    const auto& f = "Inference Rate: "
                    + std::to_string(1000000 / m_interpreter.get_prev_duration().count()) + " fps";
    cv::putText(frame, f, cv::Point(0, 20), cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5, 8, 0);

    for (const auto& [box,id] : tracked) {
      int l = static_cast<int>(box.x * m_width);
      int t = static_cast<int>(box.y * m_height);
      int r = static_cast<int>((box.x+box.width)  * m_width);
      int b = static_cast<int>((box.y+box.height) * m_height);

      cv::rectangle(frame, {l,t}, {r,b}, cvblue, 2);

      std::string text = m_track_label.count(id)
                         ? m_track_label[id] + "#" + std::to_string(id)
                         : std::string("id#") + std::to_string(id);
      cv::putText(frame, text, {l, t-6},
                  cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5);
    }

    cv::imshow("Live Inference", frame);
    cv::waitKey(1);
  }
}

DetectionCamera::~DetectionCamera() {}

}  // namespace edge
