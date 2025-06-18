#include "detection_camera.h"

#include "edgetpu.h"
#include "opencv2/opencv.hpp"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"

#include "preprocessing/edge_enhance.hpp"
#include "preprocessing/contrast.hpp"
#include "tracking/sort_tracker.hpp"

#include <chrono>
#include <algorithm>

namespace edge {

DetectionCamera::DetectionCamera(
    const std::string& model_path, const std::string& label_path, const float threshold,
    std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context, const bool edgetpu, const int source,
    const int height, const int width, const bool verbose, const std::vector<std::string>& preprocess)
    : m_interpreter(model_path, label_path, threshold, edgetpu_context, edgetpu),
      m_camera(source),
      m_height(height),
      m_width(width),
      m_verbose(verbose),
      m_preprocess(preprocess) {}

void DetectionCamera::Run() {
  const auto &in_shape = m_interpreter.GetInputShape();
  const int mdl_w = in_shape[1];
  const int mdl_h = in_shape[2];

  // ───────── camera ─────────
  if (!m_camera.isOpened()) {
    std::cerr << "Unable to open camera!\n"; return;
  }
  m_camera.set(cv::CAP_PROP_FPS, 30.0);
  m_camera.set(cv::CAP_PROP_FRAME_HEIGHT, m_height);
  m_camera.set(cv::CAP_PROP_FRAME_WIDTH,  m_width);
  m_height = static_cast<int>(m_camera.get(cv::CAP_PROP_FRAME_HEIGHT));
  m_width  = static_cast<int>(m_camera.get(cv::CAP_PROP_FRAME_WIDTH));

  const cv::Scalar RED (0,0,255), BLUE(255,0,0);

  cv::Mat frame; int fcnt = 0; std::vector<cv::Rect> roi_list;

  // alias for detection element type
  using Det = typename decltype(m_interpreter.RunInference(std::vector<uint8_t>{}))::value_type;

  while (m_camera.read(frame)) {
    ++m_frame_counter; ++fcnt;
    const bool full = (fcnt % 30 == 1) || roi_list.empty();

    std::vector<Det> dets;

    auto preprocess_rgb = [&](const cv::Mat &src, cv::Mat &dst){
      cv::Mat tmp = src;
      for(const auto &s: m_preprocess){
        if(s=="contrast") preprocessing::contrast::apply(tmp);
        else if(s=="edge") preprocessing::edge::apply(tmp);
      }
      cv::cvtColor(tmp, dst, cv::COLOR_BGR2RGB);
      cv::resize(dst, dst, {mdl_w,mdl_h});
    };

    if(full){
      cv::Mat rgb; preprocess_rgb(frame,rgb);
      std::vector<uint8_t> input(rgb.data, rgb.data+rgb.total()*rgb.elemSize());
      dets = m_interpreter.RunInference(input);
    } else {
      for(const auto &roi_orig: roi_list){
        if(roi_orig.width<4||roi_orig.height<4) continue;
        cv::Rect pad = roi_orig;
        pad.x = std::clamp(pad.x - pad.width/8, 0, frame.cols-1);
        pad.y = std::clamp(pad.y - pad.height/8,0, frame.rows-1);
        pad.width  = std::clamp(roi_orig.width*5/4,1, frame.cols-pad.x);
        pad.height = std::clamp(roi_orig.height*5/4,1, frame.rows-pad.y);
        if(pad.width<4||pad.height<4) continue;
        cv::Mat patch = frame(pad);
        cv::Mat rgb; preprocess_rgb(patch,rgb);
        std::vector<uint8_t> input(rgb.data, rgb.data+rgb.total()*rgb.elemSize());
        auto partial = m_interpreter.RunInference(input);
        for(auto &p: partial){
          p.x1 = p.x1 * pad.width  / mdl_w + pad.x;
          p.y1 = p.y1 * pad.height / mdl_h + pad.y;
          p.x2 = p.x2 * pad.width  / mdl_w + pad.x;
          p.y2 = p.y2 * pad.height / mdl_h + pad.y;
          dets.push_back(p);
        }
      }
    }

    // keep only person top‑5
    std::vector<Det> persons; persons.reserve(5);
    for(const auto &d: dets){
      if(d.candidate=="person"){ persons.push_back(d); if(persons.size()==5) break; }
    }
    dets.swap(persons);

    // tracking
    std::vector<cv::Rect2f> boxes; boxes.reserve(dets.size());
    for(const auto &d: dets) boxes.emplace_back(d.x1,d.y1,d.x2-d.x1,d.y2-d.y1);
    auto tracked = m_tracker.update(boxes);

    // map detection -> track label
    auto iou=[&](const cv::Rect2f&a,const cv::Rect2f&b){float inter=(a&b).area();float uni=a.area()+b.area()-inter;return uni>0?inter/uni:0.f;};
    for(const auto &[box,id]:tracked){
      float best=0; std::string lbl;
      for(const auto &d: dets){ cv::Rect2f r(d.x1,d.y1,d.x2-d.x1,d.y2-d.y1); float v=iou(r,box); if(v>best){best=v; lbl=d.candidate;} }
      if(best>0.3) m_track_label[id]=lbl;
    }

    // update roi list
    roi_list.clear();
    for(const auto &[box,id]:tracked){
      int l=std::clamp(int(box.x*m_width),0,m_width-1);
      int t=std::clamp(int(box.y*m_height),0,m_height-1);
      int r=std::clamp(int((box.x+box.width)*m_width),0,m_width-1);
      int b=std::clamp(int((box.y+box.height)*m_height),0,m_height-1);
      int w=r-l, h=b-t; if(w>4&&h>4) roi_list.emplace_back(l,t,w,h);
    }

    // draw
    for(const auto &[box,id]:tracked){
      int l=int(box.x*m_width); int t=int(box.y*m_height);
      int r=int((box.x+box.width)*m_width); int b=int((box.y+box.height)*m_height);
      cv::rectangle(frame,{l,t},{r,b},BLUE,2);
      std::string txt=(m_track_label.count(id)?m_track_label[id]:"id" )+"#"+std::to_string(id);
      cv::putText(frame,txt,{l,t-6},cv::FONT_HERSHEY_COMPLEX,0.8,RED,1.5);
    }

    std::string fps="Inf:"+std::to_string(int(1e6/m_interpreter.get_prev_duration().count()))+" fps";
    cv::putText(frame,fps,{0,20},cv::FONT_HERSHEY_COMPLEX,0.8,RED,1.5);

    cv::imshow("Live Inference",frame);
    cv::waitKey(1);
  }
}

DetectionCamera::~DetectionCamera(){}

} // namespace edge
