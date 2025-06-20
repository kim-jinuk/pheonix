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
#define MAX_OBJECTS 5


void Task_img_process(CaptureUnit& capunit ,ImageProcessor& imgprocessor,tracking::SortTracker& tracker, \
    edge::TfLiteWrapper& detector ,ThreadSafeQueue<FramePtr>& out_queue ,ThreadSafeQueue<SendPacket> &send_queue) {

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
            /* enhance*/
            imgprocessor.enhance_edges(frame->img_bgr);
            imgprocessor.enhance_contrast(frame->img_bgr);
        //    std::cout << "img type: " << frame->img_bgr.type() << std::endl;

            if (frame_num%5==0) {
              //  std::cout << " try push" <<std::endl;
                out_queue.push(frame);
            }
            
            // 결과 받아옴. (string ,score x1,y1,x2,y2)
            {
             std::lock_guard<std::mutex> lock(infer_mtx);
             candidates=InferResult;
            }


            std::vector<cv::Rect2f> det_boxes;
            det_boxes.reserve(candidates.size());
            // 5. tracking
            // infer 결과의 박스 정보 채움
            for (const auto& c : candidates) {
                det_boxes.emplace_back(c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);
            }
            // 박스 정보를 바탕으로 <bbox, int> 가져옴, int 는 id
            // tracked는 <bbbox, int> 저장되어있음
            const auto tracked = tracker.update(det_boxes);
            auto iou = [](const cv::Rect2f& a, const cv::Rect2f& b)
                {
                    float inter = (a & b).area();
                    float uni   = a.area() + b.area() - inter;
                    return uni>0 ? inter/uni : 0.f; 
                };
            // iou 계산해서 추론 결과에 있는 최종 cls 반영 + id에 해당하는 cls 반영(m_track_label)
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
            // tracked 
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

            // 보낼 정보 저장하기
            std::vector<ObjectInfo> objects; 
            int i=0;
            for (const auto& [box, track_id] : tracked) {
                ObjectInfo obj;

                // 1. tracking ID
                obj.tracking_id = static_cast<uint8_t>(track_id);

                // 2. Label → 클래스 ID
                auto it = m_track_label.find(track_id);
                int b= track_id;
                if (it != m_track_label.end()) {
                    const std::string& label = it->second;
                    int a =obj.cls;
                    
                //    std::cout << "[DEBUG] track_id=" << b << ", label='" << label << "'";
                    obj.cls = detector.get_class_id(label);
                //    std::cout << ", mapped cls=" << a << std::endl;

                    // std::cout << label << "cls :" << a <<std::endl;
                } else {
                //    std::cout << "[DEBUG] track_id=" << b << " not found in m_track_label!" << std::endl;
                    obj.cls = 255;
                }
                // 3. 좌표 (0~1 → pixel)
                obj.x = static_cast<int16_t>(box.x * IMG_WIDTH);
                obj.y = static_cast<int16_t>(box.y * IMG_HEIGHT);
                obj.w = static_cast<int16_t>(box.width * IMG_WIDTH);
                obj.h = static_cast<int16_t>(box.height * IMG_HEIGHT);

                // 4. confidence score  InferenceResult 기준
                if (i < candidates.size()) {
                    obj.conf = candidates[i].score;
                } 
                else {
                    obj.conf = 0.0f;
                }
                objects.push_back(obj);
                i++;
            }


             while (objects.size() < 5) {
                objects.push_back(ObjectInfo{255, 255, -1, -1, -1, -1, 0.0f});
             }

            //std::cout << "=== ObjectInfo List ===" << std::endl;
            std::cout << std::dec; 
            // for (size_t i = 0; i < objects.size(); ++i) {
            //     const auto& obj = objects[i];
            //     if (objects[i].cls==255) continue;
            //     int a=frame->frame_id, b=obj.cls,c=obj.tracking_id;
            //     std::cout << "frame_num : "<< a
            //             << "[" << i << "] "
            //             << "cls: " <<  b
            //             << ", track_id: " <<  c
            //             << ", x: " << obj.x
            //             << ", y: " << obj.y
            //             << ", w: " << obj.w
            //             << ", h: " << obj.h
            //             << ", conf: " << obj.conf
            //             << std::endl;
            // }

            if (sysInfo.current_mode==Mode::TRACKING) {
                uint8_t id=targetInfo.id.load();
            //    int idd=id;
            //    std::cout<<"target id:" <<idd<<std::endl;
                int i=0;
                for (i=0;i<MAX_OBJECTS;i++) {
                    if (objects[i].tracking_id==id) {
                        
                        int16_t center_x = objects[i].x + objects[i].w / 2;
                        int16_t center_y = objects[i].y + objects[i].h / 2;
                        int x=center_x;
                        int y=center_y;
                     //   std::cout<< "target x :"  <<x << "target y:" << y << std::endl;
                        targetInfo.setXY(center_x, center_y);
                        break;
                    }
                }
                if (i>=MAX_OBJECTS) {
                    targetInfo.setXY(MISSTARGET, MISSTARGET);
                }
                
            }
            // push to Task_sendImageMeta
            send_queue.push(SendPacket{frame, objects});
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
          //  std::cout <<"try infer..." <<std::endl;
            cv::Mat rgb_frame, resized_frame;
           // std::vector<InferenceResult> candidates;
            cv::cvtColor( frame->img_bgr, rgb_frame, cv::COLOR_BGR2RGB);
                // 2. 리사이즈
            cv::resize(rgb_frame, resized_frame, cv::Size(300, 300));
                // 3. 데이터 추출
                std::vector<uint8_t> input(
                resized_frame.data,
                resized_frame.data + resized_frame.cols * resized_frame.rows * resized_frame.elemSize());
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
