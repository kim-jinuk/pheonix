#include "detecting_csc/edgetpu_detector.hpp"
#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>
#include <vector>
#include <memory>

using namespace std::chrono;

int main() {
    std::shared_ptr<edgetpu::EdgeTpuContext> context;
    context = edgetpu::EdgeTpuManager::GetSingleton()->OpenDevice();

    edge::TfLiteWrapper detector(
        "ssd_mobilenet_v1_coco_quant_postprocess_edgetpu.tflite",
        "coco_labels.txt", 0.75f, context, true);

    // 샘플 이미지 경로
    const std::string image_path = "test.jpg";
    cv::Mat img_bgr = cv::imread(image_path);
    if (img_bgr.empty()) {
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return -1;
    }

    int frame_num = 0;
    while (true) {
        auto loop_start = steady_clock::now();

        cv::Mat rgb, resized;
        cv::cvtColor(img_bgr, rgb, cv::COLOR_BGR2RGB);
        cv::resize(rgb, resized, cv::Size(300, 300));

        std::vector<uint8_t> input(
            resized.data,
            resized.data + resized.total() * resized.elemSize());

        auto infer_start = steady_clock::now();
        std::vector<InferenceResult> candidates = detector.RunInference(input);
        auto infer_end = steady_clock::now();

        if (frame_num++ % 3 == 0) {
            std::cout << "[PERF] TPU Invoke() duration: "
                      << detector.get_prev_duration().count() << " ms\n";
            std::cout << "Frame #" << frame_num
                      << " timings (ms):\n  Inference       : "
                      << duration_cast<milliseconds>(infer_end - infer_start).count()
                      << " ms\n";
        }

     //   std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 테스트용 delay
    }

    return 0;
}
