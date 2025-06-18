#pragma once
#include <opencv2/opencv.hpp>

namespace preprocessing::contrast {
/**
 * In-place contrast enhancement.
 *  - 컬러 → Lab·L 채널 CLAHE
 *  - GRAY  → equalizeHist
 */
inline void apply(cv::Mat& img,
                  double clip_limit = 4.0,
                  cv::Size tile_grid = {32, 32})
{
    cv::Mat out;
    const cv::Mat& src = img;

    if (src.channels() == 3) {
        // ----- 컬러 프레임 -----
        cv::Mat lab;
        cv::cvtColor(src, lab,
                     src.type() == CV_8UC3 ? cv::COLOR_BGR2Lab
                                           : cv::COLOR_RGB2Lab);

        std::vector<cv::Mat> channels;
        cv::split(lab, channels);          // L, a, b

        // CLAHE(Contrast Limited Adaptive Histogram Equalization)
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clip_limit, tile_grid);
        clahe->apply(channels[0], channels[0]);

        cv::merge(channels, lab);
        cv::cvtColor(lab, out,
                     src.type() == CV_8UC3 ? cv::COLOR_Lab2BGR
                                           : cv::COLOR_Lab2RGB);
    }
    else {
        // ----- 그레이스케일 -----
        cv::equalizeHist(src, out);
    }
    img = std::move(out);
}

}  // namespace preprocessing::contrast
