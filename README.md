## 프로젝트 구조

### 📁 client_opencv

- `main` (시작점)
  - `udp_receiver()` - 영상 수신 스레드
    - UDP 바인딩 및 수신 루프
    - JPEG 종료 바이트 탐지
    - 헤더(38B) + JPEG 분리
    - 이미지 디코딩 및 객체 시각화 (OpenCV)
  - `tcp_control()` - 메인 스레드에서 사용자 입력 처리
    - TCP 연결 및 명령 전송
    - 사용자 명령:
      - '1': 모드 전환 (mode_num)
      - '2': 모터 이동 (dx, dy)
      - 'q': 종료
    - struct.pack으로 패킷 생성 및 전송
    - TCP 응답 수신 및 파싱 (5바이트)

---

### 📁 server_opencv

- `main.cpp` - 프로그램 진입점 (스레드 시작)
  - `tcp_server.hpp` → `tcp_server.cpp`
    - TCP 명령 수신 및 응답 처리
  - `udp_streaming.hpp` → `udp_streaming.cpp`
    - 카메라 영상 캡처, 객체 검출, UDP 전송

- `config.hpp` - 전역 설정값 및 상태 변수 정의

- `MobileNetSSD_deploy.prototxt` - DNN 구조 파일  
- `MobileNetSSD_deploy.caffemodel` - DNN 가중치 파일
