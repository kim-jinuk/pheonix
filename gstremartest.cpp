#include <gst/gst.h>
#include <iostream>

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    const char* target_ip = "192.168.0.10";  // 주의: "192.168.10"은 올바른 IP 아님
    int target_port = 5000;

    std::string pipeline_str =
    "v4l2src device=/dev/video0 ! "
    "image/jpeg,width=640,height=480,framerate=10/1 ! "
    "jpegparse ! "
    "jpegdec ! "
    "videoconvert ! "
    "jpegenc quality=50 ! "
    "udpsink host=" + std::string(target_ip) + " port=" + std::to_string(target_port);


    std::cout << "[Pipeline] " << pipeline_str << "\n";

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipeline_str.c_str(), &error);
    if (!pipeline) {
        std::cerr << "[Error] Failed to create pipeline: " << error->message << std::endl;
        g_clear_error(&error);
        return -1;
    }

    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[Error] Failed to start pipeline." << std::endl;
        gst_object_unref(pipeline);
        return -1;
    }

    std::cout << "[Info] Streaming MJPEG (quality=50) to " << target_ip << ":" << target_port << "\n";

    // Bus 대기
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    std::cout << "[Info] Pipeline stopped.\n";
    return 0;
}
