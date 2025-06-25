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
std::atomic<bool> infer_finished=0 ;


void Task_img_process(Logger& logger, CaptureUnit& capunit ,ImageProcessor& imgprocessor,tracking::ByteTracker& tracker, \
    edge::TfLiteWrapper& detector  ,ThreadSafeQueue<SendPacket> &send_queue) {

    uint32_t frame_num = 0;
    using BDet = tracking::Detection;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() == State::RUNNING; }); 
        }

        capunit.openCamera();
        static std::vector<uint8_t> input_buffer;
        cv::Mat ws_tmp(cv::Size(300, 300), CV_8UC3);
        std::vector<InferenceResult> filtered_candidates; // 다음 프레임에도 유지하기 위해
        tracker.init_id();
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            auto frame = std::make_shared<FrameData>();
            
            if (!capunit.capture(frame)) {
      //          std::cout << "cap failed" << std::endl;
                continue;
            }

        //    std::vector<InferenceResult> candidates;
            frame->frame_id = frame_num++;
            frame->timestamp = logger.getCurrentTimestamp();
            bool do_infer = (frame_num % INFER_PER_FRAME)==1;
            
            if (do_infer) {
              //  bdet.clear();
                filtered_candidates.clear();
                // 1. BGR 그대로 resize
                cv::Mat resized;
                cv::resize(frame->img_bgr, resized, cv::Size(300, 300), 0, 0, cv::INTER_AREA);

                // 2. 전처리도 BGR 기준으로 수행
                imgprocessor.enhance_contrast(resized);
                imgprocessor.enhance_edges(resized, ws_tmp);
                imgprocessor.enhance_dehaze(resized);

                // 3. 추론 전에만 RGB 변환
                cv::Mat rgb_input;
                cv::cvtColor(resized, rgb_input, cv::COLOR_BGR2RGB);
                
                if (input_buffer.size() != rgb_input.total() * rgb_input.elemSize())
                    input_buffer.resize(rgb_input.total() * rgb_input.elemSize());
                std::memcpy(input_buffer.data(), rgb_input.data, input_buffer.size());

                auto all_candidates = detector.RunInference(input_buffer);

                // filter
            //  std::vector<InferenceResult> filtered_candidates;
                for (const auto& c : all_candidates) {
                    if (allowed_labels.count(c.candidate)) {
                        filtered_candidates.push_back(c);
                        if (filtered_candidates.size() >= 5) break;
                    }
                }
            }
            std::vector<BDet> bdet; // tracking::Detection
            

            bdet.reserve(filtered_candidates.size());
            // bdet에 박스정보, 스코어 저장
            for (const auto& c : filtered_candidates) {
                tracking::Detection d;
                d.bbox = cv::Rect2f(
                    c.x1 * IMG_WIDTH,
                    c.y1 * IMG_HEIGHT,
                    (c.x2 - c.x1) * IMG_WIDTH,
                    (c.y2 - c.y1) * IMG_HEIGHT);
                d.score = c.score;
                bdet.push_back(d);
            }

            // detection 기반 tracks 저장
            const auto tracks = tracker.update(bdet);
           
            // ───── map detection -> stable label (lock + hysteresis) ─────
            const float IOU_THR = 0.3f;

            auto IoU = [](const cv::Rect2f &a, const cv::Rect2f &b) {
                float inter = (a & b).area();
                float uni   = a.area() + b.area() - inter;
                return uni > 0 ? inter / uni : 0.f;
            };

            std::vector<int> det_ids(bdet.size(), -1);          // detection ↔ id 매핑
            std::vector<bool> track_used(tracks.size(), false);


            for (size_t di = 0; di < bdet.size(); ++di) {
                float best = IOU_THR; int best_ti = -1;
                for (size_t ti = 0; ti < tracks.size(); ++ti) {
                    if (track_used[ti]) continue;
                    float iou = IoU(bdet[di].bbox, tracks[ti].first);
                    if (iou > best) { best = iou; best_ti = static_cast<int>(ti); }
                }
                if (best_ti >= 0) {
                    det_ids[di]        = tracks[best_ti].second;  // ID 할당
                    track_used[best_ti] = true;
                    m_track_label[tracks[best_ti].second] = filtered_candidates[di].candidate;
                }
                
            }


            const auto& cvred = cv::Scalar(0, 0, 255);
            const auto& cvblue = cv::Scalar(255, 0, 0);
            frame->img_bgr = imgprocessor.ToPseudoIR(frame->img_bgr);

            // ───────────── ID + class + score 오버레이 ───────────────
            std::vector<ObjectInfo> objects;
            for (size_t i = 0; i < bdet.size(); ++i) {
                int id = det_ids[i];
                if (id < 0) continue;                  // 매칭 실패한 박스는 건너뜀

                const auto& bb = bdet[i].bbox;         // detection 박스 그대로
                const auto& c  = filtered_candidates[i];        // class / score
                int lft = static_cast<int>(bb.x + 0.5f);
                int top = static_cast<int>(bb.y + 0.5f);
                int rgt = static_cast<int>(bb.x + bb.width  + 0.5f);
                int btm = static_cast<int>(bb.y + bb.height + 0.5f);

                cv::rectangle(frame->img_bgr, {lft, top}, {rgt, btm}, cvblue, 2, 1, 0);
                std::string tag = "ID:" + std::to_string(id) +
                                    "  "   + c.candidate ;
                cv::putText(frame->img_bgr, tag, {lft, top - 5},
                            cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5, 8, 0);
                ObjectInfo obj;
                obj.tracking_id = static_cast<uint8_t>(id);
                obj.cls = detector.get_class_id(c.candidate);
                obj.x = lft;
                obj.y = top;
                obj.w = static_cast<int16_t>(bb.width  + 0.5f);
                obj.h = static_cast<int16_t>(bb.height + 0.5f);
                obj.conf = c.score;
                objects.push_back(obj);    
            }

            logger.logMeta(frame->frame_id, frame->timestamp, objects);

            while (objects.size() < 5) {
                objects.push_back(ObjectInfo{255, 255, -1, -1, -1, -1, 0.0f});
            }

            if (sysInfo.current_mode == Mode::TRACKING) {
                uint8_t id = targetInfo.id.load();
                int i = 0;
                int ii=id;
           //     std::cout << "tracking id : " << ii <<std::endl;
                for (i = 0; i < MAX_OBJECTS; i++) {
                    if (objects[i].tracking_id == id) {
                        int16_t center_x = objects[i].x + objects[i].w / 2;
                        int16_t center_y = objects[i].y + objects[i].h / 2;
                        targetInfo.setXY(center_x, center_y);
               //         std::cout << "success find " << ii <<std::endl;
                        break;
                    }
                }
                if (i >= MAX_OBJECTS) {
                    targetInfo.setXY(MISSTARGET, MISSTARGET);
                }
            }

            
            send_queue.push(SendPacket{frame, objects});
        }
        SendPacket dummy;
        send_queue.push(dummy);
        capunit.closeCamera();
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
