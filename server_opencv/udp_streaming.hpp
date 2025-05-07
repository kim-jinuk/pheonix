#pragma once
#include "config.hpp"

/**
 * @brief UDP 영상 및 객체 정보 송신 함수
 * @details
 *  카메라에서 프레임을 캡처하고 DNN(MobileNet-SSD)을 사용해 객체를 검출한 뒤,
 *  객체 정보(헤더)와 JPEG 인코딩된 영상을 UDP로 전송합니다.
 *
 * @note
 *  - 헤더는 8바이트(magic, frame_id) + 6바이트 × 5개의 객체 정보로 구성됩니다.
 *  - 패킷은 MAX_PACKET_SIZE 단위로 분할 전송되며, 포트는 config.hpp의 UDP_PORT를 사용합니다.
 *  - OpenCV 및 DNN 모듈을 사용하므로 실행 환경에 관련 모델 및 카메라가 필요합니다.
 */
void udp_streaming();
