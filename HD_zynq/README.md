# Zybo Edge-TPU EO Vision **All-in-One** Project 🚀
> 실시간 EO 영상 캡처 → 전처리 → NPU 추론 → 추적 → GUI → 저장 + 통신 까지 “단일 파이프라인”으로 구현

[![Build](https://img.shields.io/github/actions/workflow/status/your-id/Zybo-EdgeTPU/ci.yml?branch=main)](../../actions)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## 1. 프로젝트 개요
Zybo Z7-10 보드(ARM + FPGA)와 **Google Coral USB Edge TPU**를 사용해  
EO(주간) 카메라 영상을 **30 FPS 이상**으로

1. **캡처** (V4L2)  
2. **전처리** (디블러링·초해상도)  
3. **NPU 추론** (MobileNet-SSD / YOLO-Nano)  
4. **다중 객체 추적** (KCF & SORT)  
5. **GUI 표시 + TCP/UDP 통신**  
6. **영상 저장** (MJPEG·H.264)  

까지 **원-버튼 실행**으로 처리하는 임베디드 AI 파이프라인입니다.

---

## 2. 주요 기능

| 카테고리            | 기능 요약                                             |
|---------------------|--------------------------------------------------------|
| **통신**            | TCP/UDP 소켓, RTSP 스트림, 원격 명령 수신/상태 전송        |
| **영상 캡처**       | USB UVC · MIPI-CSI, 동시 다중 카메라, 프레임 드롭 방지      |
| **전처리**          | DeblurGAN-v2 Lite, ESRGAN-tiny(선택적), 컬러 ↔ GRAY 변환  |
| **모델 추론**       | TFLite INT8 모델 → Edge-TPU 컴파일 · 실행                |
| **NPU 동작 관리**   | TPU 온도/대기시간 모니터, 오류 시 CPU 백업 모드 전환       |
| **추적 & 후처리**    | KalmanBoxTracker + IOU Hungarian, 궤적 시각화            |
| **GUI**            | Qt / C# WPF 선택 가능 (현재 Qt5 QML 샘플)                |
| **영상 저장**       | MJPEG(HW 없는 경우) / x264(HW 가속 시)                   |
| **크로스 컴파일**    | `cmake/arm-linux-gnueabihf.cmake` 툴체인, CI 빌드 x86→ARM |
| **Docker 개발**     | x86 / ARM 양쪽 이미지, Edge-TPU runtime 포함             |

---

## 3. 아키텍처 한눈에 보기

```
┌─────────────┐   shared queue   ┌─────────────┐
│ FrameCapture│  ─────────────► │  Pipeline   │
│  (Thread 1) │                 │ (Thread 2)  │
└─────────────┘                 └─────────────┘
      ▲                               │
      │USB / CSI                      │Edge-TPU
      │                               ▼
┌───────────────────────────────┐  ──────>  TCP/UDP
│  GUI & VideoWriter (Thread 3) │
└───────────────────────────────┘
```

- **Python** : 파이프라인 제어 · TPU 추론  
- **C++** : 고속 추적·그리기 → `libvision_core.so`  
- **Edge-TPU** : 모든 CNN 추론(INT8)  
- **Qt GUI** / CLI 선택 실행

---

## 4. 환경 구축

### 4-1. 로컬(Linux / WSL2)

```bash
git clone https://github.com/kim-jinuk/Zybo-EdgeTPU.git
cd Zybo-EdgeTPU && ./setup_env.sh
```

### 4-2. Docker

```bash
docker build -t zybo_eo_runtime -f docker/Dockerfile .
docker run --device /dev/video0 --device /dev/bus/usb \
           -it zybo_eo_runtime
```

---

## 5. 빌드 & 실행

```bash
# (선택) C++ 모듈 빌드
mkdir build && cd build
cmake -GNinja ..
ninja && sudo ninja install

# 모델 다운로드 & Edge-TPU 컴파일
./scripts/get_models.sh       # 예시 스크립트

# 파이프라인 실행
python scripts/run_pipeline.py --cfg config/pipeline.yaml --source 0
```

---

## 6. 디렉터리 구조

```
.
├── src/
│   ├── python/              # 파이프라인 전체 (캡처·전처리·TPU·추적)
│   └── cpp/                 # 고속 C++ 모듈 (tracker, annotator)
├── config/                  # YAML 설정 (카메라·모델·GUI 등)
├── models/                  # *.tflite / *.edgetpu.tflite
├── docker/                  # Dockerfile, build scripts
├── cmake/                   # toolchain 파일
├── tests/                   # pytest / gtest
└── README.md
```

---

## 7. 개발 워크플로 & 브랜치 전략

| 단계 | 설명 |
|------|------|
| **`feature/*` 브랜치** | 새로운 모듈·버그 픽스 구현 |
| **PR & 코드리뷰**   | flake8 / clang-format CI 통과 후 2인 승인 |
| **`dev` 브랜치**     | 통합 테스트 · HW-in-loop |
| **`main` 브랜치**    | 항상 실행 가능 · release tag 생성 |

---

## 8. 로드맵

- [ ] ❶ GUI 완성 (Qt /QML)  
- [ ] ❷ IR 채널 병합 모드  
- [ ] ❸ YOLO-Nano Edge-TPU 포팅  
- [ ] ❹ FPGA PL 로 FPS Counter IP 추가  
- [ ] ❺ H.264 HW 인코더 연동

---

## 9. FAQ

| Q | A |
|---|---|
| **Windows에서도 개발 가능?** | WSL 2 + USB-IP 또는 Docker cross-build로 *빌드*는 가능. 실물 TPU/카메라는 Linux에서 테스트 권장 |
| **Edge-TPU 외 GPU 사용?** | CPU fallback 또는 OpenCL delegate 가능, 그러나 목표 전력 ≤ 8 W라 기본은 TPU 중심 |
| **IR 영상은?** | v1.0에서는 EO 전용. 플러그인 아키텍처로 IR 센서 추가 예정 |

---

## 10. 라이선스
MIT License.  
사전 학습 모델은 각 모델 원 저작권·라이선스를 따릅니다.

---
