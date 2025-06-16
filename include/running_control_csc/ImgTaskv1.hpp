#pragma once
#include "running_control_csc/MotorControl.hpp"
#include "running_control_csc/Task.hpp"
#include "running_control_csc/BIT.hpp"
#include "running_control_csc/receiver.hpp"
#include "running_control_csc/Logger.hpp"
#include "image_processing_csc/ImageProcessor.hpp"


void Task_ImageProcessing(CaptureUnit& capunit /*,EdgeTpuDetector& detector, SortTracker& tracker*/);


void Task_cap_enhance(CaptureUnit& capunit , ThreadSafeQueue<FramePtr>& out_queue);

void Task_infer_track();

void Task_send();