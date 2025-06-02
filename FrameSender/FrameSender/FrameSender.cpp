#include "FrameSender.h"
#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

static GstElement* pipeline = nullptr;
static GstElement* appsrc = nullptr;
static bool capsSet = false;

static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data) {
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* err;
        gchar* debug;
        gst_message_parse_error(msg, &err, &debug);
        g_printerr("[GST ERROR] %s\n", err->message);
        g_error_free(err);
        g_free(debug);
        break;
    }
    default:
        break;
    }
    return TRUE;
}

void InitSender(const char* pipelineStr) {
    gst_init(nullptr, nullptr);
    GError* error = nullptr;
    pipeline = gst_parse_launch(pipelineStr, &error);
    if (!pipeline || error) {
        if (error) {
            g_printerr("GStreamer parse error: %s\n", error->message);
            g_error_free(error);
        }
        return;
    }

    appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");
    if (!appsrc) {
        g_printerr("[ERROR] Could not find appsrc named 'mysrc'\n");
        return;
    }
    g_print("[INFO] Found appsrc.\n");

    // Bus 메시지 처리 등록
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, bus_call, nullptr);
    gst_object_unref(bus);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    capsSet = false; // 초기화
}

void SendFrame(const unsigned char* jpegData, int length) {
    if (!pipeline) {
        g_printerr("[GST] pipeline NULL, Init 실패\n");
        return;
    }
    if (!appsrc) {
        std::cerr << "[ERROR] appsrc is null (초기화 실패했을 수 있음)\n";
        return;
    }

    std::vector<uchar> buf(jpegData, jpegData + length);
    cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
    if (img.empty()) {
        std::cerr << "[ERROR] imdecode 실패, JPEG 데이터가 유효하지 않음 (length: " << length << " bytes)\n";
        return;
    }

    std::cout << "[INFO] SendFrame 호출됨 - 크기: "
        << img.cols << "x" << img.rows << std::endl;

    static int last_width = 0, last_height = 0;

    std::cout << "[DEBUG] capsSet: " << capsSet
        << ", 이전 해상도: " << last_width << "x" << last_height
        << ", 현재: " << img.cols << "x" << img.rows << std::endl;

    if (!capsSet || last_width != img.cols || last_height != img.rows) {
        GstCaps* caps = gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "BGR",
            "width", G_TYPE_INT, img.cols,
            "height", G_TYPE_INT, img.rows,
            "framerate", GST_TYPE_FRACTION, 30, 1,
            nullptr);
        gst_app_src_set_caps(GST_APP_SRC(appsrc), caps);
        gst_caps_unref(caps);
        std::cout << "[INFO] appsrc caps 재설정 완료\n";
        capsSet = true;
        last_width = img.cols;
        last_height = img.rows;
    }

    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, img.total() * img.elemSize(), nullptr);
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    memcpy(map.data, img.data, img.total() * img.elemSize());
    gst_buffer_unmap(buffer, &map);

    GST_BUFFER_PTS(buffer) = gst_util_uint64_scale(g_get_monotonic_time(), GST_USECOND, 1);
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 30);

    GstFlowReturn ret;
    g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);

    if (ret != GST_FLOW_OK) {
        std::cerr << "[ERROR] push-buffer 실패: " << gst_flow_get_name(ret) << std::endl;
    }
    else {
        std::cout << "[DEBUG] push-buffer 성공\n";
    }
}

void CloseSender() {
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
}