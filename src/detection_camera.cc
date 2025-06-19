#include "detection_camera.h"                  // 클래스 선언부 헤더
#include "edgetpu.h"                           // Coral Edge TPU C++ API
#include "opencv2/opencv.hpp"                  // OpenCV 전부
#include "tensorflow/lite/interpreter.h"       // TFLite 인터프리터
#include "tensorflow/lite/model.h"             // TFLite 모델 로더
#include "preprocessing/edge_enhance.hpp"      // ★ 사용자 정의 전처리 (샤프닝)
#include "preprocessing/contrast.hpp"          // ★ 사용자 정의 전처리 (대비)
#include "tracking/sort_tracker.hpp"           // SORT 트래커

#include <chrono>                              // FPS 계산용 시간
#include <algorithm>                           // std::clamp 등

namespace edge {

// ────────────────────────────────────────────────────────────────
// IR 변환 (BGR → **Gray‑IR** 3‑채널)
//   • 히스토그램 평활화 + GRAY2BGR → 사각형 등 컬러 오버레이 가능
// ────────────────────────────────────────────────────────────────
static cv::Mat ToGrayIR(const cv::Mat &bgr) {
  cv::Mat gray, hist, out;
  cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
  cv::equalizeHist(gray, hist);                    // 밝기 대비 향상
  cv::cvtColor(hist, out, cv::COLOR_GRAY2BGR);     // 3‑채널 복원
  return out;
}

// ────── ctor ──────
DetectionCamera::DetectionCamera(
    const std::string& model_path,
    const std::string& label_path,
    const float threshold,
    std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context,
    const bool edgetpu,
    const int source,
    const int height, const int width,
    const bool verbose,
    const std::vector<std::string>& preprocess)
    : m_interpreter(model_path, label_path, threshold, edgetpu_context, edgetpu),
      m_camera(source),
      m_height(height), m_width(width),
      m_verbose(verbose), m_preprocess(preprocess) {}

// ────── 메인 루프 ──────
void DetectionCamera::Run() {
  const auto &in_shape = m_interpreter.GetInputShape();
  const int mdl_w = in_shape[1];
  const int mdl_h = in_shape[2];

  if (!m_camera.isOpened()) {
    std::cerr << "Unable to open camera!\n";
    return;
  }
  m_camera.set(cv::CAP_PROP_FPS, 30.0);
  m_camera.set(cv::CAP_PROP_FRAME_HEIGHT, m_height);
  m_camera.set(cv::CAP_PROP_FRAME_WIDTH,  m_width);
  m_height = static_cast<int>(m_camera.get(cv::CAP_PROP_FRAME_HEIGHT));
  m_width  = static_cast<int>(m_camera.get(cv::CAP_PROP_FRAME_WIDTH));

  const cv::Scalar RED (0,0,255), BLUE(255,0,0);
  cv::namedWindow("Live Inference", cv::WINDOW_AUTOSIZE);

  cv::Mat frame;
  std::vector<cv::Rect> roi_list;   // 향후 최적화에 대비해 유지

  using Det = typename decltype(m_interpreter.RunInference(std::vector<uint8_t>{}))::value_type;

  bool show_ir = false;   // 'p' 키로 토글 (RGB ↔ Gray‑IR)

  while (m_camera.read(frame)) {
    ++m_frame_counter;

    // ── 항상 전체 프레임 추론 ──
    std::vector<Det> dets;

    // 입력 전처리
    auto preprocess_rgb = [&](const cv::Mat &src, cv::Mat &dst){
      cv::Mat tmp = src.clone();
      for(const auto &s: m_preprocess){
        if(s=="contrast") preprocessing::contrast::apply(tmp);
        else if(s=="edge") preprocessing::edge::apply(tmp);
      }
      cv::cvtColor(tmp, dst, cv::COLOR_BGR2RGB);
      cv::resize(dst, dst, {mdl_w, mdl_h});
    };

    cv::Mat rgb; preprocess_rgb(frame, rgb);
    std::vector<uint8_t> input(rgb.data, rgb.data + rgb.total()*rgb.elemSize());
    dets = m_interpreter.RunInference(input);
    const float sx = float(m_width)/mdl_w;
    const float sy = float(m_height)/mdl_h;
    for(auto &d: dets){ d.x1*=sx; d.x2*=sx; d.y1*=sy; d.y2*=sy; }

    // 사람 클래스 상위 5개만 유지
    std::vector<Det> persons; persons.reserve(5);
    for(const auto &d: dets){
      if(d.candidate=="person"){ persons.push_back(d); if(persons.size()==5) break; }
    }
    dets.swap(persons);

    // SORT 추적
    std::vector<cv::Rect2f> boxes; boxes.reserve(dets.size());
    for(const auto &d: dets) boxes.emplace_back(d.x1,d.y1,d.x2-d.x1,d.y2-d.y1);
    auto tracked = m_tracker.update(boxes);

    // detection ↔ track 라벨 매칭
    auto iou = [&](const cv::Rect2f&a,const cv::Rect2f&b){
      float inter=(a&b).area(); float uni=a.area()+b.area()-inter; return uni>0?inter/uni:0.f; };
    for(const auto &[box,id]:tracked){
      float best=0; std::string lbl;
      for(const auto &d: dets){
        cv::Rect2f r(d.x1,d.y1,d.x2-d.x1,d.y2-d.y1);
        float v=iou(r,box);
        if(v>best){ best=v; lbl=d.candidate; }
      }
      if(best>0.3) m_track_label[id]=lbl;
    }

    // ROI 재계산(보류)
    roi_list.clear();

    // 시각화 (BGR 기준)
    for(const auto &[box,id]:tracked){
      cv::rectangle(frame,{int(box.x),int(box.y)}, {int(box.x+box.width),int(box.y+box.height)}, BLUE,2);
      std::string txt=(m_track_label.count(id)?m_track_label[id]:"id")+"#"+std::to_string(id);
      cv::putText(frame, txt, {int(box.x),int(box.y)-6}, cv::FONT_HERSHEY_COMPLEX,0.8,RED,1.5);
    }

    // FPS
    std::string fps="Inf:"+std::to_string(int(1e6/m_interpreter.get_prev_duration().count()))+" fps";
    cv::putText(frame,fps,{0,20},cv::FONT_HERSHEY_COMPLEX,0.8,RED,1.5);

    // 표시 모드 전환
    cv::Mat vis = show_ir ? ToGrayIR(frame) : frame;
    cv::imshow("Live Inference", vis);

    int key=cv::waitKey(1);
    if(key==27) break;           // ESC
    if(key=='p'||key=='P') show_ir = !show_ir;
  }
}

DetectionCamera::~DetectionCamera(){}

} // namespace edge
