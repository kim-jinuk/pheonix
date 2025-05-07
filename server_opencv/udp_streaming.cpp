#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>

using namespace std;
using namespace cv;
using namespace cv::dnn;

extern const int UDP_PORT;
extern const char* UDP_IP;
extern const int MAX_PACKET_SIZE;
extern const float CONF_THRES;
extern const int DETECT_INT;

/**
 * @brief 객체 검출 및 영상 전송 (UDP)
 * @details 카메라에서 프레임을 캡처하고, DNN을 통해 객체를 검출한 뒤 JPEG 압축 후 UDP로 전송.
 * 프레임마다 헤더 + 객체 정보 + JPEG 영상을 묶어 분할 전송함.
 * @throws std::runtime_error 카메라 열기 실패 또는 소켓 생성 실패 시 예외 처리 대신 종료 로그 출력
 */
void udp_streaming() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        cerr << "UDP 소켓 생성 실패" << endl;
        return;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(UDP_PORT);
    servaddr.sin_addr.s_addr = inet_addr(UDP_IP);

    // @brief 사전 학습된 MobileNetSSD 모델 로드
    Net net = readNetFromCaffe("MobileNetSSD_deploy.prototxt", "MobileNetSSD_deploy.caffemodel");

    // @brief 카메라 초기화
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "카메라 열기 실패" << endl;
        close(sock);
        return;
    }

    int frame_id = 0;

    while (true) {
        Mat frame;
        cap >> frame;
        if (frame.empty()) break;

        // @brief 객체 정보 저장용 구조체 및 리스트
        struct Obj {
            uint8_t cls, x, y, w, h;
            float conf;
        };
        vector<Obj> objs;

        // @brief 일정 주기마다 객체 검출 수행
        if (frame_id % DETECT_INT == 0) {
            Mat blob = blobFromImage(frame, 0.007843, Size(300, 300), 127.5, false);
            net.setInput(blob);
            Mat detections = net.forward();

            int h = frame.rows, w = frame.cols;
            for (int i = 0; i < detections.size[2]; ++i) {
                float* data = (float*)detections.ptr<float>(0, 0, i);
                float confidence = data[2];
                if (confidence < CONF_THRES) continue;

                int cls = static_cast<int>(data[1]);
                int x = data[3] * w;
                int y = data[4] * h;
                int x2 = data[5] * w;
                int y2 = data[6] * h;

                objs.push_back({
                    static_cast<uint8_t>(cls),
                    static_cast<uint8_t>(clamp(x, 0, 255)),
                    static_cast<uint8_t>(clamp(y, 0, 255)),
                    static_cast<uint8_t>(clamp(x2 - x, 0, 255)),
                    static_cast<uint8_t>(clamp(y2 - y, 0, 255)),
                    confidence
                });
            }

            // @brief 신뢰도 기준 정렬 후 상위 5개만 선택
            sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& b) {
                return a.conf > b.conf;
            });
            if (objs.size() > 5) objs.resize(5);
        }

        // @brief JPEG 압축
        vector<uchar> jpeg_buf;
        imencode(".jpg", frame, jpeg_buf, {IMWRITE_JPEG_QUALITY, 50});

        // @brief 헤더 구성: magic(4B) + frame_id(4B) + obj(6B * 5개 = 30B)
        vector<uint8_t> header;
        uint32_t magic = 0xDEADBEEF;
        uint32_t fid = frame_id;
        header.insert(header.end(), (uint8_t*)&magic, (uint8_t*)&magic + 4);
        header.insert(header.end(), (uint8_t*)&fid, (uint8_t*)&fid + 4);
        for (int i = 0; i < 5; ++i) {
            if (i < objs.size()) {
                header.push_back(objs[i].cls);
                header.push_back(objs[i].x);
                header.push_back(objs[i].y);
                header.push_back(objs[i].w);
                header.push_back(objs[i].h);
                header.push_back(static_cast<uint8_t>(objs[i].conf * 255));
            } else {
                for (int j = 0; j < 6; ++j) header.push_back(0);
            }
        }

        // @brief UDP 페이로드 생성 및 전송
        vector<uint8_t> payload = header;
        payload.insert(payload.end(), jpeg_buf.begin(), jpeg_buf.end());
        for (size_t i = 0; i < payload.size(); i += MAX_PACKET_SIZE) {
            size_t len = std::min(static_cast<size_t>(MAX_PACKET_SIZE), payload.size() - i);
            sendto(sock, payload.data() + i, len, 0, (sockaddr*)&servaddr, sizeof(servaddr));
        }

        frame_id++;
    }

    cap.release();
    close(sock);
    destroyAllWindows();
}
