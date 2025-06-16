#pragma once
#include <vector>

class Hungarian {
public:
    static float solve(const std::vector<std::vector<float>>& cost_matrix, std::vector<int>& assignment);
};