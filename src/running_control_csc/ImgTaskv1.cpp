#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/Logger.hpp"
#include "image_processing_csc/ImageProcessor.hpp"

/**
    병목 측정용
*/
void Task_ImageProcessing(CaptureUnit& capunit ,ImageProcessor& imgprocessor,EdgeTpuDetector& detector, SortTracker& tracker , UdpSender& sender) {
    
    std::cout << "ImageProcessing thread is created" <<std::endl;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }
        capunit.openCamera();
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            std::shared_ptr<FrameData> frame =std::make_shared<FrameData>();
            // capture
            capunit.capture(frame);
            // enhance
            imgprocessor.enhance_edges(frame->img_bgr);
            // // detect
            cv::Mat rgb_frame, resized_frame;
            // 1. BGR → RGB
            cv::cvtColor(enhanced, rgb_frame, cv::COLOR_BGR2RGB);
            // 2. 리사이즈
            cv::resize(rgb_frame, resized_frame, cv::Size(300, 300));
            // 3. 데이터 추출
            std::vector<uint8_t> input(
                resized_frame.data,
                resized_frame.data + resized_frame.cols * resized_frame.rows * resized_frame.elemSize()
            );
            // 4. 추론
            std::vector<Detection> infer_result = detector.RunInference(input);
            std::vector<cv::Rect2f> det_boxes;
            det_boxes.reserve(candidates.size());
            // 5. tracking
            for (const auto& c : candidates) {
                det_boxes.emplace_back(c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);
            }

            const auto tracked = m_tracker.update(det_boxes);
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
            const auto& f = "Inference Rate: "
                    + std::to_string(1000000 / m_interpreter.get_prev_duration().count()) + " fps";
            cv::putText(frame->img_bgr, f, cv::Point(0, 20), cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5, 8, 0);
            for (const auto& [box,id] : tracked) {
                int l = static_cast<int>(box.x * m_width);
                int t = static_cast<int>(box.y * m_height);
                int r = static_cast<int>((box.x+box.width)  * m_width);
                int b = static_cast<int>((box.y+box.height) * m_height);

                cv::rectangle(frame->img_bgr, {l,t}, {r,b}, cvblue, 2);

                std::string text = m_track_label.count(id)
                                    ? m_track_label[id] + "#" + std::to_string(id)
                                    : std::string("id#") + std::to_string(id);
                cv::putText(frame->img_bgr, text, {l, t-6},
                            cv::FONT_HERSHEY_COMPLEX, .8, cvred, 1.5);
            }

            std::vector<ObjectInfo> objs(5); //dummy
            auto packets=sender.BuildUdpPackets(frame, objs);
            sender.UdpSend(packets);
            // auto t4 = std::chrono::high_resolution_clock::now();
            // std::vector<TrackResult> track_result=tracker.update(infer_result);
            // auto t5 = std::chrono::high_resolution_clock::now();

            // if (sysInfo.current_mode.load()==Mode::TRACKING) {
            //     uint8_t target_id = targetInfo.id.load();
            //     auto it=std::find_if(track_result.begin(),track_result.end(), \
            //         [target_id](const TrackResult& t) {return t.id==target_id});

            //     if (it != track_result.end()) {
            //         int center_x = it->box.x + it->box.width / 2;
            //         int center_y = it->box.y + it->box.height / 2;
            //         targetInfo.setXY(center_x, center_y);
            //     } 
            //     // 놓쳤을 때
            //     else {
            //         targetInfo.setXY(MISSTARGET,MISSTARGET);
            //     }
            // }
            
        }
        // RUNNING -> other state 이면 cam off
        capunit.closeCamera();

    }

}



void Task_cap_enhance(CaptureUnit& capunit , ThreadSafeQueue<FramePtr>& out_queue) {
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
            capunit.capture(frame);
            DevelopUnit::enhance(frame);
            
            out_queue.push(frame);
        }
        capunit.closeCamera();
    }
}

void Task_infer_track( ThreadSafeQueue<FramePtr>& in_queue, ThreadSafeQueue<FramePtr>& out_queue,TfLiteWrapper& inference) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }

        while (sysInfo.current_state.load() == State::RUNNING) {
            
            FramePtr frame = in_queue.wait_and_pop();
            // infer

            // tracking

            // update object info

            // if tracking mode?
            if (sysInfo.current_mode.load()==Mode::TRACKING) {
                uint8_t target_id = targetInfo.id.load();
                auto it=std::find_if(track_result.begin(),track_result.end(), \
                    [target_id](const TrackResult& t) {return t.id==target_id});

                if (it != track_result.end()) {
                    int center_x = it->box.x + it->box.width / 2;
                    int center_y = it->box.y + it->box.height / 2;
                    targetInfo.setXY(center_x, center_y);
                } 
                // 놓쳤을 때
                else {
                    targetInfo.setXY(MISSTARGET,MISSTARGET);
                }
            }
            out_queue.push(frame);
        }

        in_queue.clear();
    }

}

void Task_send(ThreadSafeQueue<FramePtr>& in_queue , UdpSender& udpsender) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }

       
        while (sysInfo.current_state.load() == State::RUNNING) {
            FramePtr frame = in_queue.wait_and_pop();

            
        }
        in_queue.clear();
    }

}