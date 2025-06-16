#include "detecting_csc/edgetpu_detector.hpp"

#include <time.h>
#include <iostream>
#include <memory>
#include <vector>

#include "tensorflow/lite/builtin_op_data.h"
#include "tensorflow/lite/kernels/register.h"

#include "running_control_csc/globals.hpp"
namespace edge {

static std::map<int, std::string> ParseLabel(const std::string& label_path) {
  std::map<int, std::string> ret;
  std::ifstream label_file(label_path);
  if (!label_file.good()) return ret;

  for (std::string line; std::getline(label_file, line);) {
    std::istringstream ss(line);
    int id;
    ss >> id;
    line = std::regex_replace(line, std::regex("^[0-9]+ +"), "");
    ret.emplace(id, line);
  }

  return ret;
}

// 클래스 생성자: 모델 로드 및 인터프리터 초기화
TfLiteWrapper::TfLiteWrapper(
    const std::string& model_path, const std::string& label_path, const float threshold,
    std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context, const bool edgetpu) {

  // 모델 파일 로드
  m_model = tflite::FlatBufferModel::BuildFromFile(model_path.c_str());

  // EdgeTPU 사용 여부에 따라 인터프리터 초기화 방식 결정
  if (edgetpu && edgetpu_context) {
    InitTfLiteWrapperEdgetpu(edgetpu_context);
  } else {
    InitTfLiteWrapper();
  }

  // 추론 스레드 수 설정 (1개로 고정)
  m_interpreter->SetNumThreads(1);

  // 텐서 메모리 할당
  m_interpreter->AllocateTensors();

  // 입력 텐서의 shape 저장 (예: {1, 300, 300, 3})
  const auto* dims = m_interpreter->tensor(m_interpreter->inputs()[0])->dims;
  m_input_shape = {dims->data[0], dims->data[1], dims->data[2], dims->data[3]};

  // 출력 텐서들의 shape 정보 저장
  const auto& out_tensor_indices = m_interpreter->outputs();
  m_output_shape.resize(out_tensor_indices.size());
  for (size_t i = 0; i < out_tensor_indices.size(); i++) {
    const auto* tensor = m_interpreter->tensor(out_tensor_indices[i]);
    // 출력은 float 타입만 지원한다고 가정
    m_output_shape[i] = tensor->bytes / sizeof(float);
  }

  // 라벨 파일 파싱
  m_labels = ParseLabel(label_path);

  // confidence threshold 저장
  m_threshold = threshold;
}

// EdgeTPU용 인터프리터 초기화
void TfLiteWrapper::InitTfLiteWrapperEdgetpu(
    std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context) {
  tflite::ops::builtin::BuiltinOpResolver resolver;

  // EdgeTPU용 커스텀 연산자 등록
  resolver.AddCustom(edgetpu::kCustomOp, edgetpu::RegisterCustomOp());
  std::cout << "edgetpu::RegisterCustomOp()\n";

  // 인터프리터 빌드
  if (tflite::InterpreterBuilder(*m_model, resolver)(&m_interpreter) != kTfLiteOk) {
    std::cout << "Failed to build Interpreter\n";
    std::abort();
  }

  // EdgeTPU context 연결
  m_interpreter->SetExternalContext(kTfLiteEdgeTpuContext, edgetpu_context.get());
}

// CPU 전용 인터프리터 초기화
void TfLiteWrapper::InitTfLiteWrapper() {
  tflite::ops::builtin::BuiltinOpResolver resolver;

  // 인터프리터 빌드
  if (tflite::InterpreterBuilder(*m_model, resolver)(&m_interpreter) != kTfLiteOk) {
    std::cout << "Failed to build Interpreter\n";
    std::abort();
  }
}

// 입력 텐서 shape 반환
const std::vector<int> TfLiteWrapper::GetInputShape() {
  return m_input_shape;
}

// 추론 결과를 후처리하여 InferenceResult 구조체로 변환
const std::vector<InferenceResult> TfLiteWrapper::GetResults(
    const std::vector<std::vector<float>>& output) {

  std::vector<InferenceResult> results;
  int n = lround(output[3][0]);  // 감지된 객체 수

  for (int i = 0; i < n; i++) {
    int id = lround(output[1][i]);  // class ID
    float score = output[2][i];     // confidence score

    if (score > m_threshold) {
      InferenceResult result;
      result.candidate = m_labels.at(id);  // 라벨 이름 매핑
      result.score = score;

      // box 좌표 정규화 (0~1 사이)
      result.y1 = std::max(0.0f, output[0][4 * i]);
      result.x1 = std::max(0.0f, output[0][4 * i + 1]);
      result.y2 = std::min(1.0f, output[0][4 * i + 2]);
      result.x2 = std::min(1.0f, output[0][4 * i + 3]);

      results.push_back(result);
    }
  }

  return results;
}

// 추론 전체 실행 함수: 입력 데이터를 넣고 결과 반환
const std::vector<InferenceResult> TfLiteWrapper::RunInference(
    const std::vector<uint8_t>& input_data) {

  std::vector<std::vector<float>> output_data;
  auto start = std::chrono::system_clock::now();

  // 입력 텐서 포인터 얻어서 데이터 복사
  uint8_t* input = m_interpreter->typed_input_tensor<uint8_t>(0);
  std::memcpy(input, input_data.data(), input_data.size());

  // 추론 실행
  m_interpreter->Invoke();

  // 출력 텐서 파싱
  const auto& output_indices = m_interpreter->outputs();
  const size_t num_outputs = output_indices.size();
  output_data.resize(num_outputs);

  for (size_t i = 0; i < num_outputs; ++i) {
    const auto* out_tensor = m_interpreter->tensor(output_indices[i]);
    assert(out_tensor != nullptr);

    // float 타입인 경우에만 처리
    if (out_tensor->type == kTfLiteFloat32) {
      const float* output = m_interpreter->typed_output_tensor<float>(i);
      const size_t size = m_output_shape[i];

      output_data[i].resize(size);
      for (size_t j = 0; j < size; ++j) {
        output_data[i][j] = output[j];
      }
    } else {
      // 지원되지 않는 타입 경고
      std::cerr << "Unsupported output type: " << out_tensor->type
                << "\n Tensor Name: " << out_tensor->name;
    }
  }

  // 추론 시간 측정
  m_prev_inference_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now() - start);

  // 후처리 결과 반환
  return GetResults(output_data);
}

// 이전 추론에 걸린 시간 반환 (microsecond 단위)
std::chrono::microseconds TfLiteWrapper::get_prev_duration() const {
  return m_prev_inference_duration;
}

}  // namespace edge

std::vector<ObjectInfo> TfLiteWrapper::GetResultsAsObjectInfo(
    const std::vector<std::vector<float>>& output) {

  constexpr int kImageWidth = 640;
  constexpr int kImageHeight = 480;

  std::vector<ObjectInfo> results;
  int num_detections = lround(output[3][0]);

  for (int i = 0; i < num_detections; ++i) {
    int class_id = static_cast<int>(output[1][i]);
    float score = output[2][i];

    if (score > m_threshold) {
      float y1 = std::max(0.0f, output[0][4 * i + 0]);
      float x1 = std::max(0.0f, output[0][4 * i + 1]);
      float y2 = std::min(1.0f, output[0][4 * i + 2]);
      float x2 = std::min(1.0f, output[0][4 * i + 3]);

      int16_t x = static_cast<int16_t>(x1 * kImageWidth);
      int16_t y = static_cast<int16_t>(y1 * kImageHeight);
      int16_t w = static_cast<int16_t>((x2 - x1) * kImageWidth);
      int16_t h = static_cast<int16_t>((y2 - y1) * kImageHeight);

      ObjectInfo obj;
      obj.cls = static_cast<uint8_t>(class_id);
      obj.tracking_id = 0;
      obj.x = x;
      obj.y = y;
      obj.w = w;
      obj.h = h;
      obj.conf = score;

      results.push_back(obj);
    }
  }

  return results;
}

std::vector<ObjectInfo> TfLiteWrapper::RunInferenceObjectInfo(
    const std::vector<uint8_t>& input_data) {

  std::vector<std::vector<float>> output_data;

  auto start = std::chrono::system_clock::now();

  uint8_t* input = m_interpreter->typed_input_tensor<uint8_t>(0);
  std::memcpy(input, input_data.data(), input_data.size());
  m_interpreter->Invoke();

  const auto& output_indices = m_interpreter->outputs();
  output_data.resize(output_indices.size());

  for (size_t i = 0; i < output_indices.size(); ++i) {
    const auto* out_tensor = m_interpreter->tensor(output_indices[i]);
    assert(out_tensor != nullptr);

    if (out_tensor->type == kTfLiteFloat32) {
      const float* output = m_interpreter->typed_output_tensor<float>(i);
      output_data[i].assign(output, output + m_output_shape[i]);
    } else {
      std::cerr << "Unsupported output type: " << out_tensor->type
                << "\n Tensor Name: " << out_tensor->name;
    }
  }

  m_prev_inference_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now() - start);

  return GetResultsAsObjectInfo(output_data);
}
