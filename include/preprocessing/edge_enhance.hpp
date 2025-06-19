#pragma once
#include <opencv2/opencv.hpp>

namespace preprocessing::edge {

/**
 * 3×3 Laplacian‑sharpen (fast)
 *   kernel =
 *     0  -s  0
 *    -s 1+4s -s
 *     0  -s  0
 * strength ∈ [0,1] (default 1.0 gives 약간 강한 날카로움)
 * Gaussian/Un‑sharp 대비 2× 이상 빠르다.
 */
inline void apply(cv::Mat &img, float strength = 1.0f) {
    if (img.empty()) return;

    const float s = strength;
    static thread_local cv::Mat k;
    if (k.empty() || k.at<float>(1,1) != 1.f + 4*s) {
        k = (cv::Mat_<float>(3,3) <<
             0, -s, 0,
            -s, 1.f + 4*s, -s,
             0, -s, 0);
    }

    cv::filter2D(img, img, -1, k, {-1,-1}, 0, cv::BORDER_REPLICATE);
}

} // namespace preprocessing::edge