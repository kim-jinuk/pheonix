#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <regex>
#include <string>

#include "detection_camera.h"
#include "edgetpu.h"
#include "opencv2/opencv.hpp"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"
#include "tflite_wrapper.h"

#include "preprocessing/edge_enhance.hpp"
#include "tracking/sort_tracker.hpp"

#include <sys/stat.h>

void check_file(const char* file) {
  struct stat buf;
  if (stat(file, &buf) != 0) {
    std::cerr << file << " does not exist" << std::endl;
    exit(EXIT_FAILURE);
  }
}

void usage(char* argv[]) {
  std::cerr << "Usage: " << argv[0] << " --model model_file --labels label_file" << std::endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {

  std::string model_path_;
  std::string label_path_;

  if (argc == 5) {
    for (int i = 1; i < argc; ++i) {
      if (std::string(argv[i]) == "--model")
        model_path_ = argv[++i];
      else if (std::string(argv[i]) == "--labels")
        label_path_ = argv[++i];
      else
        usage(argv);
    }
  } else {
    usage(argv);
  }

  check_file(model_path_.c_str());
  check_file(label_path_.c_str());

  // Building Interpreter.
  const auto& model_path = model_path_;
  const auto& label_path = label_path_;
  const auto threshold = .5;
  const auto with_edgetpu = true;
  auto image_height = 480;
  auto image_width = 640;
  const auto source = 0;
  const bool verbose = false;

  // Get edgetpu context.
  std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context =
      edgetpu::EdgeTpuManager::GetSingleton()->OpenDevice();
  // Creates the detection camera instance.
  edge::DetectionCamera dc(
      model_path, label_path, threshold, edgetpu_context, with_edgetpu, source, image_height,
      image_width, verbose);
  dc.Run();

  return 0;
}
