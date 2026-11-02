#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace assignment {
using Score = std::int64_t;
using Weights = std::array<Score, 26>;
struct SubstringResult {
    Score score = 0;
    std::size_t first_start = 0, second_start = 0, length = 0;
    std::string first, second;
};
inline void validate_letters(const std::string& text) {
    for (char c : text)
        if (c < 'A' || c > 'Z') throw std::invalid_argument("strings must contain only A-Z");
}
inline SubstringResult common_substring(const std::string& first, const std::string& second,
                                        const Weights& weights, Score penalty) {
    validate_letters(first);
    validate_letters(second);
    if (penalty < 0) throw std::invalid_argument("penalty magnitude must be nonnegative");
    const Score max_weight = *std::max_element(weights.begin(), weights.end());
    if (*std::min_element(weights.begin(), weights.end()) < 0)
        throw std::invalid_argument("match weights must be nonnegative");
    if (max_weight > 0 && std::min(first.size(), second.size()) >
        static_cast<std::uint64_t>(std::numeric_limits<Score>::max() / max_weight))
        throw std::overflow_error("possible score exceeds int64 range");
    const std::size_t n = second.size();
    std::vector<Score> previous(n + 1), current(n + 1);
    std::vector<std::size_t> previous_length(n + 1), current_length(n + 1);
    SubstringResult best;
    for (std::size_t i = 1; i <= first.size(); ++i) {
        for (std::size_t j = 1; j <= n; ++j) {
            const Score contribution = first[i - 1] == second[j - 1]
                ? weights[static_cast<std::size_t>(first[i - 1] - 'A')] : -penalty;
            current[j] = std::max<Score>(0, previous[j - 1] + contribution);
            current_length[j] = current[j] > 0 ? previous_length[j - 1] + 1 : 0;
            if (current[j] > best.score) {
                best.score = current[j];
                best.length = current_length[j];
                best.first_start = i - best.length;
                best.second_start = j - best.length;
            }
        }
        previous.swap(current);
        previous_length.swap(current_length);
    }
    best.first = first.substr(best.first_start, best.length);
    best.second = second.substr(best.second_start, best.length);
    return best;
}
struct SquareResult { std::size_t row = 0, column = 0, side = 0; };
inline SquareResult largest_zero_square(const std::vector<std::uint8_t>& matrix,
                                        std::size_t rows, std::size_t columns) {
    if (columns != 0 && rows > std::numeric_limits<std::size_t>::max() / columns)
        throw std::overflow_error("matrix dimensions overflow");
    if (matrix.size() != rows * columns) throw std::invalid_argument("matrix size mismatch");
    if (std::min(rows, columns) > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("square side exceeds uint32 range");
    if (rows == 0 || columns == 0) return {};
    std::vector<std::uint32_t> dp(columns + 1);
    SquareResult best;
    for (std::size_t i = 1; i <= rows; ++i) {
        std::uint32_t diagonal = 0;
        for (std::size_t j = 1; j <= columns; ++j) {
            const auto above = dp[j];
            dp[j] = matrix[(i - 1) * columns + j - 1] != 0 ? 0
                : 1 + std::min({above, dp[j - 1], diagonal});
            if (dp[j] > best.side) best = {i - dp[j], j - dp[j], dp[j]};
            diagonal = above;
        }
    }
    return best;
}
inline constexpr std::array<Score, 26> frequency_thousandths_percent = {
    8167,1492,2782,4253,12702,2228,2015,6094,6966,153,772,4025,2406,
    6749,7507,1929,95,5987,6327,9056,2758,978,2360,150,1974,74
};
struct Scoring { Weights weights; Score penalty; double scale; };
inline Scoring scoring(int scenario, int step = 0) {
    Scoring result{};
    if (scenario == 1) {
        if (step != 0) throw std::invalid_argument("scenario 1 requires step 0");
        result.weights.fill(1); result.penalty = 1; result.scale = 1;
    } else if (scenario == 2 && step >= 1 && step <= 10) {
        for (std::size_t i = 0; i < 26; ++i)
            result.weights[i] = 11 * frequency_thousandths_percent[i];
        result.penalty = 11 * 74 + step * (12702 - 74);
        result.scale = 11000; // one reported unit is one percentage point
    } else throw std::invalid_argument("use scenario 1 or scenario 2 with step 1..10");
    return result;
}
} // namespace assignment
