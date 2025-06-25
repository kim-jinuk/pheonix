#include <opencv2/opencv.hpp>
#include <turbojpeg.h>
#include <chrono>
#include <iostream>
/**
    cv - 22ms , turbo - 19ms
*/
// TurboJPEG 압축 함수
static tjhandle tj_handle = tjInitCompress();
bool compressWithTurboJPEG(const cv::Mat& img, std::vector<uint8_t>& jpeg_buf, int quality = 30) {
    if (img.empty() || img.type() != CV_8UC3) return false;

  //  tjhandle handle = tjInitCompress();

    unsigned char* buffer = nullptr;
    unsigned long size = 0;

    int ret = tjCompress2(tj_handle,
                          img.data,
                          img.cols, 0, img.rows,
                          TJPF_BGR,
                          &buffer, &size,
                          TJSAMP_420,
                          quality,
                          TJFLAG_FASTDCT);

    if (ret == 0 && buffer) {
        jpeg_buf.assign(buffer, buffer + size);
        tjFree(buffer);
  
        return true;
    }


    return false;
}

int main() {
    cv::VideoCapture cap("/dev/video0");
    if (!cap.isOpened()) {
        std::cerr << "카메라를 열 수 없습니다." << std::endl;
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    std::cout << "[INFO] 압축 시간 비교 시작 (Press Ctrl+C to stop)\n";

    while (true) {
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) continue;

        // ----------------- imencode() 측정 ------------------
        std::vector<uchar> buf_opencv;
        auto t1 = std::chrono::high_resolution_clock::now();
        cv::imencode(".jpg", frame, buf_opencv, {cv::IMWRITE_JPEG_QUALITY, 40});
        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration_cv = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        // ----------------- TurboJPEG 측정 -------------------
        std::vector<uint8_t> buf_turbo;
        auto t3 = std::chrono::high_resolution_clock::now();
        compressWithTurboJPEG(frame, buf_turbo, 40);
        auto t4 = std::chrono::high_resolution_clock::now();
        auto duration_turbo = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();

        // ----------------- 결과 출력 -----------------------
        std::cout << "[imencode()] " << duration_cv << " ms, [TurboJPEG] " << duration_turbo
                  << " ms | 크기: " << buf_opencv.size() << " / " << buf_turbo.size() << "\n";

       
    }

    return 0;
}
