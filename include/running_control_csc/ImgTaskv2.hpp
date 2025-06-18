#pragma once
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/Task.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/Logger.hpp"
#include "running_control_csc/sender.hpp"
#include "image_processing_csc/ImageProcessor.hpp"
#include "detecting_csc/edgetpu_detector.hpp"
#include "tracking_csc/sort_tracker.hpp"
#include "edgetpu.h"

void Task_img_process(CaptureUnit& capunit ,ImageProcessor& imgprocessor, UdpSender& sender,tracking::SortTracker& tracker, ThreadSafeQueue<FramePtr>& out_queue);
void Task_infer( edge::TfLiteWrapper& detector, ThreadSafeQueue<FramePtr>& in_queue);
