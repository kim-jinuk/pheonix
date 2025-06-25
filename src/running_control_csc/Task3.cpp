
#include "running_control_csc/Task.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/sender.hpp"
#include "running_control_csc/globals.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/Logger.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
#include "running_control_csc/MotorControl.hpp"
#include "image_processing_csc/ImageProcessor.hpp"
#include "detecting_csc/edgetpu_detector.hpp"
#include "tracking_csc/sort_tracker.hpp"
#include "edgetpu.h"
#include "opencv2/opencv.hpp"
#include <vector>
#include <iostream>
#include <atomic>
#define IMG_WIDTH 640
#define IMG_HEIGHT 480
#define MAX_OBJECTS 5

/**
    주기 통신, Tcpsate 전송
    Checking -> IDLE , Checking ->IDLE , RUNNING -> CHECKING
*/
void Task_sendState(TcpStateChannel& tcpStateChannel , BIT& bit ,Logger& logger ) {
    

    while (true) {
        
        if (!tcpStateChannel.AcceptConnection()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        sysInfo.TCP_state_connected.store(true);
        while (sysInfo.TCP_state_connected.load()==true) {
            bit.cbit();
            TcpState tcpstate;
            // 0 CHECKING , 1 IDLE , 2 RUNNGING
            tcpstate.state_num=static_cast<uint8_t>(sysInfo.current_state.load());
            tcpstate.tpu=sysInfo.TPU_state;
            tcpstate.cam=sysInfo.CAM_state;
            tcpstate.sdcard=sysInfo.logging_enabled;
            tcpstate.cpu_temp=sysInfo.cpu_temp;
            
            // change state depending on device
            if (!(sysInfo.TPU_state && sysInfo.CAM_state)) {
                sysInfo.current_state.store(State::CHECKING);
                tcpstate.state_num=static_cast<int>(State::CHECKING);
            }
            // device ok
            else {
                if (tcpstate.state_num==static_cast<uint8_t>(State::CHECKING)) {
                    sysInfo.current_state.store(State::IDLE);
                    tcpstate.state_num=static_cast<uint8_t>(State::IDLE);
                }
            }
            tcpstate.mode_num=static_cast<uint8_t>(sysInfo.current_mode.load());

            if (tcpstate.state_num!=static_cast<int>(State::CHECKING))
            {
                std::lock_guard<std::mutex> lock(pos_mtx);
                tcpstate.Nx=pos.yaw;
                tcpstate.Ny=pos.pitch;
            }

           // std::cout << "state :"<< tcpstate.state_num<< " mode :" <<tcpstate.mode_num <<std::endl;
            if (!tcpStateChannel.sendState(tcpstate)) {
                sysInfo.TCP_state_connected.store(false); 
             //   std::cout <<"send state failed" <<std::endl;
                sysInfo.current_state.store(State::CHECKING);
                break;
            }
            
            logger.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
          //  std::cout <<"send loop doing..." <<std::endl;
        }

    }

}

/**
    명령 송신
    IDLE -> RUNNING , RUNNING -> IDLE
*/
void Task_receiveCmd(TcpCmdChannel& tcpCmdChannel , MotorControl& motorcontrol,Logger& logger) {

    while (true) {
        
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait_for(lock,std::chrono::seconds(1), [] \
            { return sysInfo.current_state.load() \
                ==State::IDLE; }); 
        }
    //    std::cout <<" wake up CMD thread" << std::endl;
        if (!tcpCmdChannel.AcceptConnection()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        sysInfo.TCP_cmd_connected.store(true);
        // IDLE 상태에서 tcp 요청받으면 IDLE -> RUNNING
        {
            std::lock_guard<std::mutex> lock(statesync.mtx);
            sysInfo.current_state.store(State::RUNNING);
            sysInfo.current_mode.store(Mode::MANUAL);
           // logger.logStateChange(State_str[state],State_str[static_cast<int>(State::RUNNING)]);
        }
        statesync.cv.notify_all();
        
        motorcontrol.setStrategy(static_cast<uint8_t>(Mode::MANUAL));
        {
            std::lock_guard<std::mutex> lock(pos_mtx);
            pos.yaw=90;
            pos.pitch=90;
        }
     //   std::cout << "set MANUAL motor" << std::endl;
        while (sysInfo.current_state.load()==State::RUNNING) {
            TcpCommand cmd;
      //      std::cout << "CMD thread Wait CMD... " << std::endl;
            
            if (!tcpCmdChannel.TcpParsing(cmd)) {
                sysInfo.TCP_cmd_connected.store(false); 
                sysInfo.current_state.store(State::IDLE);
                break;
            }
            // std::cout << "flag : " << static_cast<int>(cmd.cmd_flag) \
            // << " cmd : " << static_cast<int>(cmd.cmd) << std::endl;
            switch(cmd.cmd_flag) {
                case Mode_num :
         //           std::cout<< "change mode"<<std::endl;
                    sysInfo.current_mode.store(static_cast<Mode>(cmd.cmd));
                    motorcontrol.setStrategy(cmd.cmd);
                    break;
                case Cam_num :
         //           std::cout<< "change EO/IR"<<std::endl;
                    cam_opt.eo_ir.store(cmd.cmd);

                    break;
                case Prep_opt : {
           //         std::cout<< "set Prep_opt"<<std::endl;
                    cam_opt.fromCmd(cmd.cmd);
                    int a=cam_opt.enhance_edges.load();
                    int b=cam_opt.enhance_contrast.load();
             //       std::cout<< "edges : "<< a << " contrast : "<< b << std::endl;
                    break;
                }
                case move_motor :
                    if (sysInfo.current_mode.load() == Mode::MANUAL) {
                        motorcontrol.enqueueDeltaIfManual(cmd.cmd);
                    }
                    break;
                case track :
                    int aaa;
                    targetInfo.id.store(cmd.cmd);
                    aaa=cmd.cmd;
            //        std::cout<< "do tracking id :"<< aaa <<std::endl;
                    sysInfo.current_mode.store(Mode::TRACKING);
                    motorcontrol.setStrategy(static_cast<uint8_t>(Mode::TRACKING));
                    break;
                case InitMotor :
                    if (sysInfo.current_mode.load()==(Mode::MANUAL)) {
                        std::lock_guard<std::mutex> lock(pos_mtx);
                        pos.yaw=90;
                        pos.pitch=90;
                    }
                default :
                    break;
            }
            logger.logCmd(cmd.cmd_flag,cmd.cmd);
            // ack
            if (!tcpCmdChannel.sendAck(cmd)) {
                sysInfo.TCP_cmd_connected.store(false); 
                sysInfo.current_state.store(State::IDLE);
                break;
            }

        }
        // 장치 이상으로 (다른 스레드가 RUNNING에서 다른 상태로 보내면) 기존 소켓 닫기
        tcpCmdChannel.disconnect_sock();
        motorcontrol.setStrategy(static_cast<uint8_t>(Mode::DEFAULT));
  //      std::cout << "set DEFAULt motor" << std::endl;
       // std::cout << "TCP cmd channel disconnected, thread sleep 0.3s..." << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

}


void Task_moveMotor(MotorControl& motorcontrol) {
    
  //  std::cout << "motor thread is created" <<std::endl;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] \
            { return sysInfo.current_state.load() \
                ==State::RUNNING; }); 
        }
     //   std::cout << "motor thread wake up" <<std::endl;
        motorcontrol.init_pos();
        while (sysInfo.current_state.load() == State::RUNNING) {
            
            motorcontrol.runStrategy();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }


}

struct CachedDet {
    cv::Rect2f box;
    float      score;
    std::string label;
    int        age;
};

constexpr int   MAX_TTL     = INFER_PER_FRAME + 1; // 최소 1-2프레임 여유
constexpr float MIN_SCORE   = 0.15f;               // 감쇠 후 버릴 기준
constexpr float DECAY_RATIO = 0.96f;               // 프레임마다 score *= 0.95

// ─── 메인 Task ──────────────────────────────────────────────────
void Task_img_process(Logger& logger,
                      CaptureUnit& capunit,
                      ImageProcessor& imgprocessor,
                      tracking::ByteTracker& tracker,
                      edge::TfLiteWrapper& detector,
                      ThreadSafeQueue<FramePtr>& out_queue,
                      ThreadSafeQueue<SendPacket>& send_queue)
{
    uint32_t frame_num = 0;
    using BDet = tracking::Detection;

    std::vector<CachedDet> det_cache;          // ★ 캐시(프레임 간 유지)

    while (true)
    {
        {   // RUNNING 대기
            std::unique_lock<std::mutex> lock(statesync.mtx);
            statesync.cv.wait(lock, [] { return sysInfo.current_state.load()==State::RUNNING; });
        }

        capunit.openCamera();

        while (sysInfo.current_state.load()==State::RUNNING)
        {
            auto frame = std::make_shared<FrameData>();
            if (!capunit.capture(frame)) continue;

            frame->frame_id  = frame_num++;
            frame->timestamp = logger.getCurrentTimestamp();

            // ─── 1. 추론 프레임 결정 ──────────────────────────
            bool do_infer = (frame_num % INFER_PER_FRAME == 1);
            if (do_infer) out_queue.push(frame);

            // ─── 2. 추론 결과 수령 → 캐시 교체 ───────────────
            std::vector<InferenceResult> candidates;
            if (infer_finished.load())
            {
                std::lock_guard<std::mutex> lg(infer_mtx);
                candidates = std::move(InferResult);     // 최신 결과
                infer_finished.store(false);

                // ★ 캐시 초기화
                det_cache.clear();
                for (const auto& d : candidates)
                    if (d.score >= MIN_SCORE)
                        det_cache.push_back({ {d.x1,d.y1,d.x2-d.x1,d.y2-d.y1},
                                             d.score, d.candidate, 0 });
            }
            else
            {
                // ★ 새 추론이 없으면 캐시 감쇠 & TTL 적용
                for (auto& c : det_cache) {
                    c.age   ++;
                    c.score *= DECAY_RATIO;
                }
                det_cache.erase(
                    std::remove_if(det_cache.begin(), det_cache.end(),
                                   [&](const CachedDet& c){
                                         return c.age > MAX_TTL || c.score < MIN_SCORE;
                                   }),
                    det_cache.end());

                // 캐시를 candidates 형태로 변환
                candidates.reserve(det_cache.size());
                for (const auto& c : det_cache) {
                    InferenceResult ir;
                    ir.x1 = c.box.x;                ir.y1 = c.box.y;
                    ir.x2 = c.box.x + c.box.width;  ir.y2 = c.box.y + c.box.height;
                    ir.score = c.score;
                    ir.candidate = c.label;
                    candidates.push_back(std::move(ir));
                }
            }

            // ─── 3. detection → ByteTracker 입력(bdet) ──────
            std::vector<BDet> bdet;
            for (auto& d : candidates)
                if (d.score >= 0.3f)
                    bdet.push_back({ {d.x1,d.y1,d.x2-d.x1,d.y2-d.y1}, d.score });

            auto tracked = tracker.update(bdet);

            // ─── 4. 라벨 매칭 & 히스테리시스 ───────────────
          //  const float IOU_THR = 0.3f;
            const float IOU_THR = 0.4f;
            auto iou = [&](const cv::Rect2f& a,const cv::Rect2f& b){
                float inter=(a&b).area(); float uni=a.area()+b.area()-inter;
                return uni>0 ? inter/uni : 0.f;
            };

            for (const auto& [box,id] : tracked)
            {
                float best=0.f; std::string cand;
                for (const auto& d : candidates){
                    cv::Rect2f r(d.x1,d.y1,d.x2-d.x1,d.y2-d.y1);
                    float v=iou(r,box);
                    if (v>best){ best=v; cand=d.candidate; }
                }
                if (best<IOU_THR) continue;

                auto& st = m_label_state[id];
                if (st.cls.empty())            { st.cls=cand; st.votes=1; }
                else if (cand==st.cls)         { st.votes=std::min(st.votes+1,10); }
                else if (--st.votes<=-5)       { st.cls=cand; st.votes=1; }
                m_track_label[id]=st.cls;
            }

            // ─── 5. 시각화 & ObjectInfo 작성 (변경 없음) ──────
            frame->img_bgr = imgprocessor.ToPseudoIR(frame->img_bgr);
            const auto cvred  = cv::Scalar(0,0,255), cvblue=cv::Scalar(255,0,0);

            std::unordered_map<int,float> id2conf;          // ★ 정확 confidence
            for (const auto& d : candidates){
                cv::Rect2f r(d.x1,d.y1,d.x2-d.x1,d.y2-d.y1);
                for (const auto& [box,id]:tracked)
                    if (iou(r,box) > IOU_THR) id2conf[id]=d.score;
            }

            std::vector<ObjectInfo> objects;
            for (const auto& [box,id] : tracked)
            {
                int l=box.x * IMG_WIDTH,  t=box.y * IMG_HEIGHT;
                int r=(box.x+box.width)*IMG_WIDTH, b=(box.y+box.height)*IMG_HEIGHT;

                cv::rectangle(frame->img_bgr,{l,t},{r,b},cvblue,2);
                std::string text = m_track_label.count(id)
                                   ? m_track_label[id] + "#" + std::to_string(id)
                                   : "id#" + std::to_string(id);
                cv::putText(frame->img_bgr,text,{l,t-6},
                            cv::FONT_HERSHEY_PLAIN,1.6,cvred,1.5);

                ObjectInfo obj;
                obj.tracking_id = static_cast<uint8_t>(id);
                obj.cls = m_track_label.count(id)
                          ? detector.get_class_id(m_track_label[id]) : 255;
                obj.x = l; obj.y = t; obj.w = r-l; obj.h = b-t;
                obj.conf = id2conf.count(id) ? id2conf[id] : 0.f;
                objects.push_back(obj);
            }
            while(objects.size()<5) objects.push_back({255,255,-1,-1,-1,-1,0.f});

            logger.logMeta(frame->frame_id, frame->timestamp, objects);

            // ─── 6. TRACKING 모드 좌표 출력 & 전송 ───────────
            if (sysInfo.current_mode == Mode::TRACKING)
            {
                uint8_t tid = targetInfo.id.load();
                auto it = std::find_if(objects.begin(), objects.end(),
                                       [&](const ObjectInfo& o){return o.tracking_id==tid;});
                if (it!=objects.end())
                    targetInfo.setXY(it->x+it->w/2, it->y+it->h/2);
                else
                    targetInfo.setXY(MISSTARGET, MISSTARGET);
            }

            send_queue.push({frame,objects});
        } // while RUNNING

        capunit.closeCamera();
    } // while(true)
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
