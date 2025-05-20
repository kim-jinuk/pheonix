# EOIR Zynq 통신 인터페이스

이 프로젝트는 Zynq 기반 임베디드 장치와 원격 PC 간의 UDP/TCP 통신 시스템을 구현합니다. 실시간 모터 제어와 영상 스트리밍을 지원하며, EO/IR 카메라 시스템을 위한 PWM 기반 모터 제어, MJPEG 영상 스트리밍, 각도 피드백 기능을 포함합니다.

## 🔧 주요 기능

* **MJPEG 실시간 영상 스트리밍 (UDP 기반)**
* **양방향 모터 제어 (TCP 소켓 기반)**
* **PWM 신호 생성 (X/Y 축)**
* **각도 피드백 전송 (0.5초 간격)**
* **멀티스레드 구조로 동시 처리 가능**
---

## 🛠️ 빌드 방법 (Linux)

source /opt/petalinux/2022.1/environment-setup-cortexa9t2hf-neon-xilinx-linux-gnueabi

${CXX} main_toGui_udp_develop.cpp -o main_toGui_udp_develop `pkg-config --cflags --libs opencv4` -pthread

./main_toGui_develop

---

## 📁 파일 구성

```
comm_test_final/
├── main_toGui_develop.cpp   # 주요 통신 및 제어 코드
├── config.txt               # IP 및 포트 설정파일
```

---

## 📄 config.txt 파일 형식

설정값은 다음 형식으로 저장됩니다:

```
UDP_IP=192.168.1.10
UDP_PORT=5000
TCP_PORT=9999
ANGLE_TCP_PORT=9998
```

* `UDP_IP`: 영상 수신 PC의 IP 주소
* `UDP_PORT`: MJPEG 영상 스트림에 사용할 포트
* `TCP_PORT`: 모터 제어 명령 수신용 포트
* `ANGLE_TCP_PORT`: 각도 피드백 전송용 포트

---

## 🚦 PWM 모터 제어

* **PWM 채널 0, 1**을 사용해 두 개의 축(X/Y)을 제어합니다
* Linux sysfs 기반 PWM 제어:

  * 주기: `40ms` (25Hz)
  * 듀티: 각도(0\~180도)에 따라 선형 매핑

---

## 🎥 MJPEG 영상 스트리밍

* `/dev/video0` 카메라로부터 V4L2를 통해 MJPEG 프레임 캡처
* 1400바이트 단위로 UDP 전송하며, 8바이트 헤더 포함:

```
[0-3] Frame ID (uint32_t)
[4-5] Packet ID (uint16_t)
[6-7] Total Packets (uint16_t)
[8- ] JPEG 데이터
```

---

## 🎮 모터 제어 프로토콜

* 총 6바이트의 제어 명령을 TCP로 수신:

```
[0-1] MAGIC_WORD (0xA5A5)
[2-3] 모드 번호 (사용 안함)
[4]   dx (int8_t)
[5]   dy (int8_t)
```

* `dx`, `dy` 값만큼 현재 angle\_x, angle\_y에 더해 PWM 갱신

---

## 📡 각도 피드백 프로토콜

* TCP 연결이 활성화되면 0.5초 간격으로 각도를 전송:

```
[0]  0xA5
[1]  0x5A
[2]  angle_x (uint8_t)
[3]  angle_y (uint8_t)
[4]  0x0D
[5]  0x0A
```

---

## 🧵 멀티스레드 구조

`std::thread`를 이용해 아래 3개의 스레드가 동시에 실행됩니다:

* `udp_sender`: MJPEG 영상 송출
* `tcp_receiver`: 모터 제어 명령 수신 및 PWM 제어
* `angle_sender`: 각도 주기 전송

모든 스레드는 `std::condition_variable`을 사용해 TCP 연결 상태에 따라 동작 여부를 결정합니다.

---

## 📌 작성자 및 목적

* 작성자: 문수빈 (Subeen Moon)
* 목적: 단이단 Zynq 통신 시스템 인터페이스 제공공용
* 플랫폼: Zynq 기반 임베디드 리눅스 시스템

---
