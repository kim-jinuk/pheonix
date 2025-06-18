#pragma once
#include <opencv2/opencv.hpp>
#include "preprocessing/contrast.hpp"

namespace preprocessing::edge {
/**
 * In-place un-sharp-mask edge enhancement.
 */
inline void apply(cv::Mat& img, float strength = 1.0f)
{
    cv::Mat blur;
    cv::GaussianBlur(img, blur, {0,0}, 3);
    cv::addWeighted(img, 1.0f + strength, blur, -strength, 0, img);
}
}   // namespace preprocessing::edge