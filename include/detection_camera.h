#ifndef EDGETPU_TFLITE_CV_DETECTION_CAMERA_H_
#define EDGETPU_TFLITE_CV_DETECTION_CAMERA_H_

#include "edgetpu.h"
#include "opencv2/opencv.hpp"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"
#include "tflite_wrapper.h"
#include "tracking/sort_tracker.hpp"
#include <unordered_map>

namespace edge {

class DetectionCamera {
public:
  // Constructors.
  DetectionCamera(
      const std::string& model, const std::string& label_path, const float threshold,
      std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context, const bool edgetpu,
      const int source, const int height, const int width, const bool verbose,
      const std::vector<std::string>& prep);
  DetectionCamera() = delete;
  // Loops camera frame and performs inference on each frame.
  void Run();
  // Destructor.
  ~DetectionCamera();

private:
  TfLiteWrapper m_interpreter;
  cv::VideoCapture m_camera;
  tracking::SortTracker    m_tracker{0.3f};
  std::unordered_map<int,std::string> m_track_label;
  std::vector<std::string> m_preprocess;
  int m_height;
  int m_width;
  const bool m_verbose;
  size_t m_frame_counter{0};
};

}  // namespace edge
#endif  // EDGETPU_TFLITE_CV_TFLITE_WRAPPER_H
