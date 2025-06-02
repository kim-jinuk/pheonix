#include <gst/gst.h>
#include <iostream>
#include <csignal>

static gboolean quit = FALSE;

void intHandler(int) { quit = TRUE; }   // Ctrl+C 신호

int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    const char* outfile = (argc > 1) ? argv[1] : "capture.mp4";

    /* ★ 파이프라인 문자열 ★ */
    std::string pipeline_str =
        "udpsrc port=5005 buffer-size=524288 caps=\"application/x-rtp, "
        "media=video, encoding-name=H264, payload=96\" "
        "! rtph264depay "
        "! h264parse config-interval=-1 "
        "! mp4mux faststart=true "
        "! filesink location=" + std::string(outfile);

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(pipeline_str.c_str(), &err);
    if (!pipeline) {
        std::cerr << "[GST] 파이프라인 생성 실패: "
            << (err ? err->message : "unknown") << std::endl;
        if (err) g_error_free(err);
        return -1;
    }

    /* 버스 메시지 모니터 */
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    std::cout << "[INFO] 녹화 시작 → " << outfile << std::endl;

    /* Ctrl+C 핸들러 등록 */
    signal(SIGINT, intHandler);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    std::cout << "[INFO] 녹화 중, Ctrl+C 로 종료" << std::endl;

    /* 메인 루프 */
    while (!quit) {
        GstMessage* msg = gst_bus_timed_pop_filtered(
            bus, 100 * GST_MSECOND,
            (GstMessageType)(GST_MESSAGE_ERROR));

        if (msg) {
            switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError* err; gchar* dbg;
                gst_message_parse_error(msg, &err, &dbg);
                std::cerr << "[GST ERROR] " << err->message << std::endl;
                g_error_free(err); g_free(dbg);
                //terminate = TRUE;
                break;
            }
            case GST_MESSAGE_EOS:
                std::cout << "[INFO] EOS 수신, 종료" << std::endl;
                //terminate = TRUE;
                break;
            default:
                break;
            }
            gst_message_unref(msg);
        }
    }

    /* ① EOS 이벤트 전송 */
    gst_element_send_event(pipeline, gst_event_new_eos());

    /* ② EOS 메시지 올 때까지 대기 → moov atom 작성 */
    gboolean eos_done = FALSE;
    while (!eos_done) {
        GstMessage* msg = gst_bus_timed_pop_filtered(
            bus, GST_CLOCK_TIME_NONE,
            (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        if (msg) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) eos_done = TRUE;
            gst_message_unref(msg);
        }
    }

    /* ③ 정리 */
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);

    std::cout << "[INFO] 완료!" << std::endl;
    return 0;
}
