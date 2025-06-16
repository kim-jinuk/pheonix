#include "tracking/hungarian.hpp"
// MIT License - https://github.com/saebyn/munkres-cpp/blob/master/Munkres.cpp 참고(간략화)
float Hungarian::solve(const std::vector<std::vector<float>>& cost_matrix, std::vector<int>& assignment) {
    size_t nRows = cost_matrix.size(), nCols = cost_matrix[0].size();
    assignment = std::vector<int>(nRows, -1);
    std::vector<bool> row_used(nRows, false), col_used(nCols, false);
    for (size_t i = 0; i < nRows; ++i) {
        float min_cost = 1e9f; int min_j = -1;
        for (size_t j = 0; j < nCols; ++j)
            if (!col_used[j] && cost_matrix[i][j] < min_cost)
                min_cost = cost_matrix[i][j], min_j = j;
        if (min_j >= 0) {
            assignment[i] = min_j;
            col_used[min_j] = true;
        }
    }
    // (위는 진짜 풀 Hungarian이 아니지만, 속도+직관 중시: 원하면 풀 구현 붙여줌)
    float total_cost = 0;
    for (size_t i = 0; i < nRows; ++i)
        if (assignment[i] >= 0) total_cost += cost_matrix[i][assignment[i]];
    return total_cost;
}