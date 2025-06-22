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
#include <atomic>
#define IMG_WIDTH 640
#define IMG_HEIGHT 480
#define MAX_OBJECTS 5

int x=0;
std::atomic<bool> infer_finished=false;
void Task_img_process(Logger& logger, CaptureUnit& capunit ,ImageProcessor& imgprocessor,tracking::SortTracker& tracker, \
    edge::TfLiteWrapper& detector ,ThreadSafeQueue<FramePtr>& out_queue ,ThreadSafeQueue<SendPacket> &send_queue) {

    uint32_t frame_num = 0;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() == State::RUNNING; }); 
        }
        capunit.openCamera();
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            auto frame = std::make_shared<FrameData>();

            if (!capunit.capture(frame)) {
                std::cout << "cap failed" << std::endl;
                continue;
            }

            std::vector<InferenceResult> candidates;
            frame->frame_id = frame_num++;
            frame->timestamp = logger.getCurrentTimestamp();
            const bool do_infer = (frame_num % INFER_PER_FRAME == 0); 
          //  bool use_infer = (frame_num % INFER_PER_FRAME) < 3; //uuupp
            bool use_infer = (frame_num % 2) == 0; //upupup
            if (do_infer) {
                out_queue.push(frame);  // 추론 프레임만 전송
            }
            if (infer_finished.load())
            {

                std::lock_guard<std::mutex> lock(infer_mtx);
                candidates = InferResult;
              //  std::cout << frame->frame_id << " uses result of frame " << x << std::endl;
                infer_finished.store(false);
            }

            std::vector<std::pair<cv::Rect2f, int>> tracked;
            std::vector<cv::Rect2f> det_boxes;

            if (use_infer) {
              //  std::cout << "frame : " << frame->frame_id << "use update" <<std::endl;
                //  추론 프레임인 경우: update() 수행
                det_boxes.reserve(candidates.size());
                for (const auto& c : candidates) {
                    det_boxes.emplace_back(c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);
                }
                tracked = tracker.update(det_boxes);

                //  class 매핑 수행
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
            } else {
                //추론 없는 경우: predict_only() 사용
                tracked = tracker.predict_only();
               // std::cout << "frame : " << frame->frame_id << "use predict" <<std::endl;
            }

            const auto& cvred = cv::Scalar(0, 0, 255);
            const auto& cvblue = cv::Scalar(255, 0, 0);
            frame->img_bgr = imgprocessor.ToPseudoIR(frame->img_bgr);

            for (const auto& [box, id] : tracked) {
                int l = static_cast<int>(box.x * IMG_WIDTH);
                int t = static_cast<int>(box.y * IMG_HEIGHT);
                int r = static_cast<int>((box.x + box.width) * IMG_WIDTH);
                int b = static_cast<int>((box.y + box.height) * IMG_HEIGHT);

                cv::rectangle(frame->img_bgr, {l, t}, {r, b}, cvblue, 2);

                std::string text = m_track_label.count(id)
                    ? m_track_label[id] + "#" + std::to_string(id)
                    : std::string("id#") + std::to_string(id);
                cv::putText(frame->img_bgr, text, {l, t - 6},
                    cv::FONT_HERSHEY_PLAIN, 1.6, cvred, 1.5);
            }

            // ObjectInfo 구성
            std::vector<ObjectInfo> objects;
            int i = 0;
            for (const auto& [box, track_id] : tracked) {
                ObjectInfo obj;
                obj.tracking_id = static_cast<uint8_t>(track_id);

                auto it = m_track_label.find(track_id);
                if (it != m_track_label.end()) {
                    const std::string& label = it->second;
                    obj.cls = detector.get_class_id(label);
                } else {
                    obj.cls = 255;
                }

                obj.x = static_cast<int16_t>(box.x * IMG_WIDTH);
                obj.y = static_cast<int16_t>(box.y * IMG_HEIGHT);
                obj.w = static_cast<int16_t>(box.width * IMG_WIDTH);
                obj.h = static_cast<int16_t>(box.height * IMG_HEIGHT);

                if (i < candidates.size()) {
                    obj.conf = candidates[i].score;
                } else {
                    obj.conf = 0.0f;
                }
                objects.push_back(obj);
                i++;
            }

            logger.logMeta(frame->frame_id, frame->timestamp, objects);

            while (objects.size() < 5) {
                objects.push_back(ObjectInfo{255, 255, -1, -1, -1, -1, 0.0f});
            }

            if (sysInfo.current_mode == Mode::TRACKING) {
                uint8_t id = targetInfo.id.load();
                int i = 0;
                for (i = 0; i < MAX_OBJECTS; i++) {
                    if (objects[i].tracking_id == id) {
                        int16_t center_x = objects[i].x + objects[i].w / 2;
                        int16_t center_y = objects[i].y + objects[i].h / 2;
                        targetInfo.setXY(center_x, center_y);
                        break;
                    }
                }
                if (i >= MAX_OBJECTS) {
                    targetInfo.setXY(MISSTARGET, MISSTARGET);
                }
            }

            
            send_queue.push(SendPacket{frame, objects});
        }

        capunit.closeCamera();
    }
}



void Task_infer(ImageProcessor& imgprocessor ,edge::TfLiteWrapper& detector, ThreadSafeQueue<FramePtr>& in_queue) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }
        cv::Mat ws_tmp(cv::Size(300, 300), CV_8UC3);
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            FramePtr frame = in_queue.wait_and_pop();
          //  std::cout <<"try infer..." <<std::endl;
            cv::Mat rgb_frame, resized_frame;
            cv::resize(frame->img_bgr, resized_frame, cv::Size(300, 300), 0, 0, cv::INTER_LINEAR);
            imgprocessor.enhance_contrast(resized_frame);
            imgprocessor.enhance_edges(resized_frame, ws_tmp);
            imgprocessor.enhance_dehaze(resized_frame);

            // std::vector<InferenceResult> candidates;
            cv::cvtColor( resized_frame, rgb_frame, cv::COLOR_BGR2RGB);
            
            std::vector<uint8_t> input(
            rgb_frame.data,
            rgb_frame.data + rgb_frame.cols * rgb_frame.rows * rgb_frame.elemSize());
            auto all_candidates = detector.RunInference(input);

            std::vector<InferenceResult> filtered_candidates;
            for (const auto& c : all_candidates) {
                if (allowed_labels.count(c.candidate)) {
                    filtered_candidates.push_back(c);
                    if (filtered_candidates.size() >= 5) break;
                }
            }
            
            {
             std::lock_guard<std::mutex> lock(infer_mtx);
             InferResult = std::move(filtered_candidates);
             x=frame->frame_id;
             infer_finished.store(true);
            }        
        }
        in_queue.clear();
    }

}

void Task_sendImageMeta( UdpSender& sender,ThreadSafeQueue<SendPacket>& in_queue) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }

        while (sysInfo.current_state.load() == State::RUNNING) {
            SendPacket pkt = in_queue.wait_and_pop();
            auto packets = sender.BuildUdpPackets(*pkt.frame, pkt.objects);
            sender.UdpSend(packets);
        }
        in_queue.clear();
    }

}
