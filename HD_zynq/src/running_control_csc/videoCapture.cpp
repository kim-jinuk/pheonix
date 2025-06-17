#include "running_control_csc/videoCapture.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>

#define DEVICE "/dev/video0"
#define WIDTH 640
#define HEIGHT 480
#define BUFFER_COUNT 2

void DisableUvcAutoControls(int fd) {
    struct v4l2_control control;

    // 자동 화이트밸런스 끄기
    control.id = V4L2_CID_AUTO_WHITE_BALANCE;
    control.value = 0;
    if (ioctl(fd, VIDIOC_S_CTRL, &control) < 0)
        perror("[WARN] AUTO_WHITE_BALANCE");

    // 자동 노출 끄기 (1 = manual)
    control.id = V4L2_CID_EXPOSURE_AUTO;
    control.value = 1; // V4L2_EXPOSURE_MANUAL
    if (ioctl(fd, VIDIOC_S_CTRL, &control) < 0)
        perror("[WARN] AUTO_EXPOSURE");

    // 자동 포커스 끄기
    control.id = V4L2_CID_FOCUS_AUTO;
    control.value = 0;
    if (ioctl(fd, VIDIOC_S_CTRL, &control) < 0)
        perror("[WARN] AUTO_FOCUS");
}

std::vector<uint8_t> CaptureFrame() {
    struct Buffer { void* start; size_t length; };
    static int fd = -1;
    static Buffer buffers[BUFFER_COUNT];
    static bool initialized = false;

    if (!initialized) {
        fd = open(DEVICE, O_RDWR);
        if (fd < 0) {
            perror("[ERR] open camera");
            return {};
        }

        struct v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = WIDTH;
        fmt.fmt.pix.height = HEIGHT;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        ioctl(fd, VIDIOC_S_FMT, &fmt);

        struct v4l2_requestbuffers req{};
        req.count = BUFFER_COUNT;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        ioctl(fd, VIDIOC_REQBUFS, &req);

        for (int i = 0; i < BUFFER_COUNT; ++i) {
            struct v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(fd, VIDIOC_QUERYBUF, &buf);

            buffers[i].length = buf.length;
            buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        }

        for (int i = 0; i < BUFFER_COUNT; ++i) {
            struct v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(fd, VIDIOC_QBUF, &buf);
        }

        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMON, &type);

        // UVC 자동 제어 끄기
        // DisableUvcAutoControls(fd);

        initialized = true;
    }

    struct v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("[ERR] DQBUF");
        return {};
    }

    std::vector<uint8_t> frame((uint8_t*)buffers[buf.index].start, (uint8_t*)buffers[buf.index].start + buf.bytesused);

    // MJPEG → BGR 디코딩
    cv::Mat decoded = cv::imdecode(frame, cv::IMREAD_COLOR);
    if (decoded.empty()) {
        std::cerr << "[ERR] imdecode failed" << std::endl;
        return {};
    }

    // 필요한 경우 리사이즈 또는 전처리 가능
    // cv::resize(decoded, decoded, cv::Size(WIDTH / 2, HEIGHT / 2));

    // BGR → JPEG 재압축
    std::vector<uint8_t> compressed;
    std::vector<int> encode_params = { cv::IMWRITE_JPEG_QUALITY, 80 }; // 품질 조정 가능
    auto start = std::chrono::high_resolution_clock::now();

    if (!cv::imencode(".jpg", decoded, compressed, encode_params)) {
        std::cerr << "[ERR] imencode failed" << std::endl;
        return {};
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 출력
    std::cout << "[INFO] JPEG 압축 시간: " << duration << " ms" << std::endl;

    ioctl(fd, VIDIOC_QBUF, &buf);
    return compressed;
}
