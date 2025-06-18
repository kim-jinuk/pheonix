#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/Logger.hpp"
#include "running_control_csc/sender.hpp"
#include "running_control_csc/globals.hpp"
#include "image_processing_csc/ImageProcessor.hpp"
#include "detecting_csc/edgetpu_detector.hpp"
#include "tracking_csc/sort_tracker.hpp"
#include "edgetpu.h"
#include "opencv2/opencv.hpp"
#include <vector>
#include <thread>
#include <iostream>
#define IMG_WIDTH 640
#define IMG_HEIGHT 480



void Task_img_process(CaptureUnit& capunit ,ImageProcessor& imgprocessor, UdpSender& sender,tracking::SortTracker& tracker, ThreadSafeQueue<FramePtr>& out_queue) {
    uint32_t frame_num=0;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }
        capunit.openCamera();
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            auto frame = std::make_shared<FrameData>();

            if (!capunit.capture(frame)) {
                std::cout << "cap failed" <<std::endl;
                continue;
            }
            std::vector<InferenceResult> candidates;
            frame->frame_id=frame_num++;
            frame->img_bgr = imgprocessor.enhance_edges(frame->img_bgr);

            if (frame_num%3==0) {
                std::cout << " try push" <<std::endl;
                out_queue.push(frame);
            }
            
            {
             std::lock_guard<std::mutex> lock(infer_mtx);
             candidates=InferResult;
            }

            std::vector<cv::Rect2f> det_boxes;
            det_boxes.reserve(candidates.size());
            // 5. tracking
            for (const auto& c : candidates) {
                det_boxes.emplace_back(c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);
            }
            const auto tracked = tracker.update(det_boxes);
            auto iou = [](const cv::Rect2f& a, const cv::Rect2f& b)
                {
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


            const auto& cvred = cv::Scalar(0, 0, 255);
            const auto& cvblue = cv::Scalar(255, 0, 0);
            const auto& f = "Inference Rate: ";
                  //  + std::to_string(1000000 / detector.get_prev_duration().count()) + " fps";
            cv::putText(frame->img_bgr, f, cv::Point(0, 20), cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5, 8, 0);
            for (const auto& [box,id] : tracked) {
                int l = static_cast<int>(box.x * IMG_WIDTH);
                int t = static_cast<int>(box.y * IMG_HEIGHT);
                int r = static_cast<int>((box.x+box.width)  * IMG_WIDTH);
                int b = static_cast<int>((box.y+box.height) * IMG_HEIGHT);

                cv::rectangle(frame->img_bgr, {l,t}, {r,b}, cvblue, 2);

                std::string text = m_track_label.count(id)
                                    ? m_track_label[id] + "#" + std::to_string(id)
                                    : std::string("id#") + std::to_string(id);
                cv::putText(frame->img_bgr, text, {l, t-6},
                            cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5);
            }

            std::vector<ObjectInfo> objs(5); //dummy
            auto packets=sender.BuildUdpPackets(*frame, objs);
            sender.UdpSend(packets);


        }

        capunit.closeCamera();
    }
}


void Task_infer( edge::TfLiteWrapper& detector, ThreadSafeQueue<FramePtr>& in_queue) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }

        while (sysInfo.current_state.load() == State::RUNNING) {
            
            FramePtr frame = in_queue.wait_and_pop();
            std::cout <<"try infer..." <<std::endl;
            cv::Mat rgb_frame, resized_frame;
           // std::vector<InferenceResult> candidates;
            cv::cvtColor( frame->img_bgr, rgb_frame, cv::COLOR_BGR2RGB);
                // 2. 리사이즈
            cv::resize(rgb_frame, resized_frame, cv::Size(300, 300));
                // 3. 데이터 추출
                std::vector<uint8_t> input(
                resized_frame.data,
                resized_frame.data + resized_frame.cols * resized_frame.rows * resized_frame.elemSize());
            auto candidates = detector.RunInference(input);
                    std::cout <<"finish infer..." <<std::endl;
            {
             std::lock_guard<std::mutex> lock(infer_mtx);
             InferResult=candidates;
            }
            
        }
        in_queue.clear();
    }

}
