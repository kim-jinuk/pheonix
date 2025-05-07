#pragma once
#include "config.hpp"

/**
 * @brief TCP 서버 실행 함수
 * @details 클라이언트로부터 명령을 수신하고, 모터 제어 또는 모드 전환을 처리한 뒤 응답을 전송합니다.
 * 이 함수는 멀티스레딩 환경에서 실행되며, 전역 변수(current_angle_x, current_angle_y, prev_mode)를 갱신합니다.
 *
 * @note 서버는 포트 9999에서 요청을 수신합니다.
 */
void tcp_server();
