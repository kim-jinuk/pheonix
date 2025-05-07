import socket
import struct
import threading
import numpy as np
import cv2

UDP_IP = "0.0.0.0"
UDP_PORT = 5005
TCP_IP = "192.168.222.131"
TCP_PORT = 9999
MAX_PACKET_SIZE = 1400
JPEG_EOF = b'\xff\xd9'
HEADER_SIZE = 38

prev_mode = 0  # 이전 모드 추적용


 # @brief 헤더 바이트에서 객체 정보 파싱
 # @details 객체의 클래스, 위치, 크기, 신뢰도를 추출하여 리스트로 반환
 # @param header_bytes 바이트 배열 (38바이트)
 # @return list 튜플 리스트 [(cls, x, y, w, h, conf), ...]
 
def parse_objects_from_header(header_bytes):
    objects = []
    for i in range(5):
        offset = 8 + i * 6
        cls = header_bytes[offset]
        x = header_bytes[offset + 1]
        y = header_bytes[offset + 2]
        w = header_bytes[offset + 3]
        h = header_bytes[offset + 4]
        conf = header_bytes[offset + 5] / 255.0
        if conf > 0:
            objects.append((cls, x, y, w, h, conf))
    return objects


# @brief UDP를 통해 영상 및 헤더 수신
# @details JPEG 영상 스트림과 헤더를 수신하고, 객체 정보를 영상 위에 시각화
# @throws socket.error 소켓 바인딩 또는 수신 중 에러 발생 가능

def udp_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    print("UDP 수신 대기 중...")

    buffer = b''
    while True:
        try:
            packet, _ = sock.recvfrom(MAX_PACKET_SIZE)
            buffer += packet

            if JPEG_EOF in packet:
                if len(buffer) > HEADER_SIZE:
                    header = buffer[:HEADER_SIZE]
                    jpeg = buffer[HEADER_SIZE:]
                    img = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)

                    if img is not None:
                        objects = parse_objects_from_header(header)
                        for i, (cls, x, y, w, h, conf) in enumerate(objects):
                            label = f"#{i} C{cls} ({conf:.2f})"
                            cv2.rectangle(img, (x, y), (x + w, y + h), (0, 255, 0), 2)
                            cv2.putText(img, label, (x, y - 5), cv2.FONT_HERSHEY_SIMPLEX,
                                        0.5, (0, 255, 0), 1)
                        cv2.imshow("UDP Video", img)
                        if cv2.waitKey(1) & 0xFF == ord('q'):
                            break
                buffer = b''
        except:
            break

    sock.close()
    cv2.destroyAllWindows()


 # @brief TCP 명령 전송 및 응답 수신
 # @details 사용자 입력을 받아 모드 전환 또는 모터 이동 명령을 전송하고, 응답을 파싱하여 출력
 # @throws Exception 연결 실패 또는 수신 오류 발생 가능
 
def tcp_control():
    global prev_mode
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        print("TCP 연결 시도 중...")
        sock.connect((TCP_IP, TCP_PORT))
        print("TCP 연결됨")

        while True:
            print("\n|| 명령 입력:")
            print("||    [1] 모드 전환 (0 : Scan, 1 : Detection, 2 : Tracking)")
            print("||    [2] 모터 이동 (dx, dy 입력)")
            print("||    [q] 종료")

            cmd = input("  선택 ▶ ").strip()
            if cmd == '1':
                mode = int(input("  전환할 mode_num ▶ "))
                packet = struct.pack('<HBH', 0xA5A5, 1, mode)
                sock.sendall(packet)
            elif cmd == '2':
                dx = int(input("    dx (-90 ~ 90) ▶ "))
                dy = int(input("    dy (-90 ~ 90) ▶ "))
                packet = struct.pack('<HBbb', 0xA5A5, 0, dx, dy)
                sock.sendall(packet)
            elif cmd.lower() == 'q':
                print("종료됨")
                break
            else:
                print("잘못된 입력입니다.")
                continue

            # 응답 수신
            resp = sock.recv(5)
            if len(resp) == 5:
                r_magic, r_flag = struct.unpack('<HB', resp[:3])
                if r_magic == 0x5A5A:
                    if r_flag == 1:
                        motor_x = struct.unpack('b', resp[3:4])[0]
                        motor_y = struct.unpack('b', resp[4:5])[0]
                        if prev_mode == 0 and mode in [1, 2]:
                            print(f"\n[응답] 모드 전환 완료: mode = {mode} → 현재 모터 각도: x = {motor_x}, y = {motor_y}")
                        else:
                            print(f"\n[응답] 모드 전환 완료: mode = {mode}")
                        prev_mode = mode
                    elif r_flag == 0:
                        motor_x, motor_y = struct.unpack('bb', resp[3:])
                        print(f"\n[응답] 현재 모터 각도: x = {motor_x}, y = {motor_y}")
                else:
                    print(f"magic word 불일치: {hex(r_magic)}")
            else:
                print("응답 길이 오류")

    except Exception as e:
        print("TCP 오류:", e)
    finally:
        sock.close()

if __name__ == "__main__":
    threading.Thread(target=udp_receiver, daemon=True).start()
    tcp_control()
