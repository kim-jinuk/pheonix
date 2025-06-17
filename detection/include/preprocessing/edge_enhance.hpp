#pragma once
#include <opencv2/opencv.hpp>
#include "preprocessing/contrast.hpp"

namespace preprocessing {
/**
 * Simple unsharp‑mask edge enhancement.
 */
inline cv::Mat enhance_edges(const cv::Mat &src, float strength = 1.0f) {
    cv::Mat blur;
    cv::GaussianBlur(src, blur, {0,0}, 3);
    cv::Mat sharp;
    cv::addWeighted(src, 1.0 + strength, blur, -strength, 0, sharp);
    return sharp;
}

/**
 * Convenience composition – runs contrast then edge.
 */
inline cv::Mat apply(const cv::Mat &frame) {
    return enhance_edges(enhance_contrast(frame));
}
}