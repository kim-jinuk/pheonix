// sender_gst_turbo.cpp
#include <opencv2/opencv.hpp>
#include <turbojpeg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <iostream>

#define DEST_IP   "192.168.1.2"
#define DEST_PORT 5000
#define MAX_FRAMES 100
// 인코딩 시간 cv와 거의 비슷...
int main() {
    std::string pipeline =
        "v4l2src device=/dev/video0 ! "
        "video/x-raw,format=YUY2,width=640,height=480,framerate=30/1 ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! appsink";

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "[ERR] Failed to open camera with GStreamer pipeline\n";
        return -1;
    }

    // TurboJPEG 초기화
    tjhandle tj_compressor = tjInitCompress();
    if (!tj_compressor) {
        std::cerr << "[ERR] Failed to initialize TurboJPEG\n";
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest.sin_addr);

    for (int quality : {30, 40, 50, 60, 70, 80, 90}) {
        std::cout << "\n=== Testing JPEG_QUALITY = " << quality << " ===\n";

        for (int frame_id = 0; frame_id < MAX_FRAMES; ++frame_id) {
            auto t0 = std::chrono::high_resolution_clock::now();

            cv::Mat frame;
            cap >> frame;
            if (frame.empty()) continue;

            auto t1 = std::chrono::high_resolution_clock::now();

            unsigned char* jpegBuf = nullptr;
            unsigned long jpegSize = 0;

            int ret = tjCompress2(
                tj_compressor,
                frame.data,
                frame.cols,
                frame.step,
                frame.rows,
                TJPF_BGR,
                &jpegBuf,
                &jpegSize,
                TJSAMP_420,
                quality,
                TJFLAG_FASTDCT
            );

            auto t2 = std::chrono::high_resolution_clock::now();

            if (ret != 0 || jpegSize > 60000) {
                std::cerr << "[WARN] JPEG error or frame too large: " << tjGetErrorStr() << ", size: " << jpegSize << "\n";
                tjFree(jpegBuf);
                continue;
            }

            sendto(sock, jpegBuf, jpegSize, 0, (sockaddr*)&dest, sizeof(dest));
            auto t3 = std::chrono::high_resolution_clock::now();

            double dec_time   = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double enc_time   = std::chrono::duration<double, std::milli>(t2 - t1).count();
            double send_time  = std::chrono::duration<double, std::milli>(t3 - t2).count();
            double total_time = std::chrono::duration<double, std::milli>(t3 - t0).count();

            std::cout << "[Q" << quality << "] Frame " << frame_id
                      << " | dec: " << dec_time << " ms"
                      << " | Enc: " << enc_time << " ms"
                      << " | Send: " << send_time << " ms"
                      << " | Total: " << total_time << " ms"
                      << " | Size: " << jpegSize << " bytes\n";

            tjFree(jpegBuf);  // 압축 버퍼 해제
        }
    }

    close(sock);
    tjDestroy(tj_compressor);
    return 0;
}
