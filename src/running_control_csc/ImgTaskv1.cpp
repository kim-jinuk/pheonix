#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/Logger.hpp"
#include "image_processing_csc/ImageProcessor.hpp"

/**
    병목 측정용
*/
void Task_ImageProcessing(CaptureUnit& capunit /*,EdgeTpuDetector& detector, SortTracker& tracker*/) {
    
    std::cout << "ImageProcessing thread is created" <<std::endl;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }

        std::cout << "ImageProcessingor thread wake up" <<std::endl;
        capunit.openCamera();

        while (sysInfo.current_state.load() == State::RUNNING) {
            
            std::shared_ptr<FrameData> frame =std::make_shared<FrameData>();
            // capture
            auto t1 = std::chrono::high_resolution_clock::now();
            capunit.capture(frame);
            // enhance
            auto t2 = std::chrono::high_resolution_clock::now();
            DevelopUnit::enhance(frame);
            // // detect
            // auto t3 = std::chrono::high_resolution_clock::now();
            // std::vector<Detection> infer_result=detector.infer(frame.img_bgr);
            // // track
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

            // // box
            // for (const auto& t : track_result)
            //     cv::rectangle(frame.img_bgr, t.box, {0,255,0}, 2);
            // cv::Rect dummy_box(100, 100, 50, 50);
            // cv::rectangle(frame.img_bgr, dummy_box, {0, 255, 0}, 2);

            // std::cout << "capture: " << (t2-t1) << "ms, enhance: " << (t3-t2)
            // << "ms, infer: " << (t4-t3) << "ms, track: " << (t5-t4) << "ms" << std::endl;
            /**
                압축
            */
            std::vector<uint8_t> encoded;
            // cv::imencode(".jpg", frame.img_bgr, encoded);
            // ObjectInfo obj = {1, 1, dummy_box.x, dummy_box.y, dummy_box.width, dummy_box.height, 0.9f};
            
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
            auto t2 = std::chrono::high_resolution_clock::now();
            DevelopUnit::enhance(frame);
            out_queue.push(frame);
        }
        capunit.closeCamera();
    }
}

void Task_infer_track( ThreadSafeQueue<FramePtr>& in_queue, ThreadSafeQueue<FramePtr>& out_queue/*추론, 트랙 객체*/) {
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