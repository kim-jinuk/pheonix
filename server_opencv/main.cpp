#include <thread>
#include <cstdint>
#include "tcp_server.hpp"
#include "udp_streaming.hpp"
#include "config.hpp"

/**
 * @brief 현재 모터의 X축 각도
 * @details -90 ~ 90 범위 내에서 이동 가능
 */
int8_t current_angle_x = 0;

/**
 * @brief 현재 모터의 Y축 각도
 * @details -90 ~ 90 범위 내에서 이동 가능
 */
int8_t current_angle_y = 0;

/**
 * @brief 이전에 설정된 동작 모드
 * @details 0: Scan, 1: Detection, 2: Tracking
 */
uint16_t prev_mode = 0;

/**
 * @brief UDP 통신에 사용되는 포트 번호
 */
const int UDP_PORT = 5005;

/**
 * @brief UDP 통신 대상 IP 주소 (Host PC)
 */
const char* UDP_IP = "192.168.222.1";

/**
 * @brief UDP 패킷의 최대 크기
 */
const int MAX_PACKET_SIZE = 1400;

/**
 * @brief 객체 검출의 신뢰도 임계값
 * @details 이 값보다 낮은 신뢰도는 무시함
 */
const float CONF_THRES = 0.5;

/**
 * @brief 객체 검출 수행 간격 (프레임 단위)
 * @details DETECT_INT 간격으로만 DNN 객체 검출 수행
 */
const int DETECT_INT = 1;

/**
 * @brief 프로그램 시작점
 * @details TCP 서버 스레드와 UDP 스트리밍 스레드를 각각 실행
 * @return 성공 시 0 반환
 */
int main() {
    std::thread tcp_thread(tcp_server);
    std::thread udp_thread(udp_streaming);

    tcp_thread.join();
    udp_thread.join();

    return 0;
}
