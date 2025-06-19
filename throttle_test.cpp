#include "image_processing_csc/ImageProcessor.hpp"
#include "running_control_csc/globals.hpp"
#include "detecting_csc/edgetpu_detector.hpp"
#include "tracking_csc/sort_tracker.hpp"
#include "running_control_csc/sender.hpp"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>

#define IMG_WIDTH 640
#define IMG_HEIGHT 480

using namespace std::chrono;
std::vector<InferenceResult> candidates;
int main() {
    CaptureUnit cap;
    ImageProcessor imgproc;
    std::shared_ptr<edgetpu::EdgeTpuContext> context;
    
    context = edgetpu::EdgeTpuManager::GetSingleton()->OpenDevice();
    
    edge::TfLiteWrapper detector("mobilenet_ssd_v2_coco_quant_postprocess_edgetpu.tflite", "coco_labels.txt", 0.75f, context, true);
    tracking::SortTracker tracker(0.3f); // IoU 임계값
    UdpSender sender("192.168.1.100", 5000); // 적절한 IP/포트로 설정

    std::unordered_map<int, std::string> m_track_label;

    if (!cap.openCamera()) return -1;

    uint32_t frame_num = 0;

    while (true) {
        auto loop_start = steady_clock::now();

        auto frame = std::make_shared<FrameData>();

        auto t0 = steady_clock::now();
        if (!cap.capture(frame)) continue;
        auto t1 = steady_clock::now();

        imgproc.enhance_edges(frame->img_bgr);
        auto t2 = steady_clock::now();

        imgproc.enhance_contrast(frame->img_bgr);
        auto t3 = steady_clock::now();

        
        auto infer_start = steady_clock::now();

        if (frame_num++ % 3 == 0) {
            cv::Mat rgb, resized;
            cv::cvtColor(frame->img_bgr, rgb, cv::COLOR_BGR2RGB);
            cv::resize(rgb, resized, cv::Size(300, 300));

            std::vector<uint8_t> input(
                resized.data,
                resized.data + resized.total() * resized.elemSize());

            candidates = detector.RunInference(input);
        }

        auto infer_end = steady_clock::now();

        std::vector<cv::Rect2f> det_boxes;
        for (const auto& c : candidates)
            det_boxes.emplace_back(c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);

        auto track_start = steady_clock::now();
        const auto tracked = tracker.update(det_boxes);
        

        // 추적 ID와 추론 결과 매칭
        auto iou = [](const cv::Rect2f& a, const cv::Rect2f& b) {
            float inter = (a & b).area();
            float uni   = a.area() + b.area() - inter;
            return uni > 0 ? inter / uni : 0.f;
        };

        for (size_t i = 0; i < tracked.size(); ++i) {
            int id = tracked[i].second;
            float best = 0.f;
            std::string best_label;
            for (const auto& c : candidates) {
                cv::Rect2f r(c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);
                float v = iou(r, tracked[i].first);
                if (v > best) { best = v; best_label = c.candidate; }
            }
            if (best > 0.3f) m_track_label[id] = best_label;
        }

        auto track_end = steady_clock::now();
        // 오버레이
        const auto& cvred = cv::Scalar(0, 0, 255);
        const auto& cvblue = cv::Scalar(255, 0, 0);
        std::string fps_str = "Inference Rate: " +
            std::to_string(1000000 / detector.get_prev_duration().count()) + " fps";

        cv::putText(frame->img_bgr, fps_str, cv::Point(0, 20), cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5);

        for (const auto& [box, id] : tracked) {
            int l = static_cast<int>(box.x * IMG_WIDTH);
            int t = static_cast<int>(box.y * IMG_HEIGHT);
            int r = static_cast<int>((box.x + box.width) * IMG_WIDTH);
            int b = static_cast<int>((box.y + box.height) * IMG_HEIGHT);

            cv::rectangle(frame->img_bgr, {l, t}, {r, b}, cvblue, 2);
            std::string text = m_track_label.count(id)
                ? m_track_label[id] + "#" + std::to_string(id)
                : "id#" + std::to_string(id);
            cv::putText(frame->img_bgr, text, {l, t - 6}, cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5);
        }
        auto end_overlay = steady_clock::now();
        // UDP 전송
        std::vector<ObjectInfo> objs(5); // 더미
        auto send_start = steady_clock::now();
        auto packets = sender.BuildUdpPackets(*frame, objs);
        sender.UdpSend(packets);
        auto send_end = steady_clock::now();

        // 시간 출력
        std::cout << "Frame #" << frame->frame_id << " timings (ms):\n";
        std::cout << "  Capture         : " << duration_cast<milliseconds>(t1 - t0).count() << " ms\n";
        std::cout << "  Enhance edges   : " << duration_cast<milliseconds>(t2 - t1).count() << " ms\n";
        std::cout << "  Enhance contrast: " << duration_cast<milliseconds>(t3 - t2).count() << " ms\n";
        if (frame_num % 3 == 1) {
            std::cout << "  Inference       : " << duration_cast<milliseconds>(infer_end - infer_start).count() << " ms\n";
        }
        std::cout << "  Tracking        : " << duration_cast<milliseconds>(track_end - track_start).count() << " ms\n";
        std::cout << "  Overaly        : " << duration_cast<milliseconds>(end_overlay - track_end).count() << " ms\n";
        std::cout << "  UDP Send        : " << duration_cast<milliseconds>(send_end - send_start).count() << " ms\n";
        std::cout << "  Total loop      : " << duration_cast<milliseconds>(steady_clock::now() - loop_start).count() << " ms\n\n";
    }   

    cap.closeCamera();
    return 0;
}
