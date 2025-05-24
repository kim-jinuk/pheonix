
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cstring>
#include <iostream>
#include <vector>
#include "UdpSender.hpp"

#define DEVICE "/dev/video0"
#define WIDTH 640
#define HEIGHT 480
#define BUFFER_COUNT 2
#define MAX_PAYLOAD_SIZE 1400

std::vector<uint8_t> UdpSender::UdpPayload() {
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
        initialized = true;
    }

    struct v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("[ERR] DQBUF");
        return {};
    }

    uint8_t* data = static_cast<uint8_t*>(buffers[buf.index].start);
    size_t size = buf.bytesused;
    std::vector<uint8_t> frame(data, data + size);

    ioctl(fd, VIDIOC_QBUF, &buf);
    return frame;
}
