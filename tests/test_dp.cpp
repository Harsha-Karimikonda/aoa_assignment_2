#include "../src/dp.hpp"
#include <iostream>
#include <random>
#include <functional>

using namespace assignment;
std::size_t checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "invalid input accepted");
}
Score exhaustive_substring(const std::string& a, const std::string& b,
                            const Weights& weights, Score penalty) {
    long double best = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = 0; j < b.size(); ++j) {
            long double total = 0;
            for (std::size_t k = 0; i + k < a.size() && j + k < b.size(); ++k) {
                total += a[i + k] == b[j + k] ? weights[static_cast<std::size_t>(a[i + k] - 'A')] : -penalty;
                best = std::max(best, total);
            }
        }
    return static_cast<Score>(best);
}
void check_substring(const std::string& a, const std::string& b, const Weights& w, Score p) {
    const auto answer = common_substring(a, b, w, p);
    require(answer.score == exhaustive_substring(a, b, w, p), "substring optimum differs");
    require(answer.first_start + answer.length <= a.size(), "first range invalid");
    require(answer.second_start + answer.length <= b.size(), "second range invalid");
    require(answer.first == a.substr(answer.first_start, answer.length), "first extraction differs");
    require(answer.second == b.substr(answer.second_start, answer.length), "second extraction differs");
    Score score = 0;
    for (std::size_t k = 0; k < answer.length; ++k)
        score += answer.first[k] == answer.second[k] ? w[static_cast<std::size_t>(answer.first[k] - 'A')] : -p;
    require(score == answer.score, "extracted score differs");
}
std::size_t exhaustive_square(const std::vector<std::uint8_t>& b, std::size_t m, std::size_t n) {
    std::size_t best = 0;
    for (std::size_t row = 0; row < m; ++row)
        for (std::size_t col = 0; col < n; ++col)
            for (std::size_t side = 1; side <= std::min(m - row, n - col); ++side) {
                bool zero = true;
                for (std::size_t i = row; i < row + side; ++i)
                    for (std::size_t j = col; j < col + side; ++j)
                        if (b[i * n + j]) zero = false;
                if (zero) best = std::max(best, side);
            }
    return best;
}
void check_square(const std::vector<std::uint8_t>& b, std::size_t m, std::size_t n) {
    const auto answer = largest_zero_square(b, m, n);
    require(answer.side == exhaustive_square(b, m, n), "square optimum differs");
    require(answer.row + answer.side <= m && answer.column + answer.side <= n, "square range invalid");
    for (std::size_t i = answer.row; i < answer.row + answer.side; ++i)
        for (std::size_t j = answer.column; j < answer.column + answer.side; ++j)
            require(b[i * n + j] == 0, "extracted square has nonzero");
}
int main() {
    try {
        const auto unit = scoring(1);
        for (const auto& pair : std::vector<std::pair<std::string, std::string>>{
                {"", ""}, {"", "ABC"}, {"ABC", ""}, {"ABC", "ABC"}, {"AAA", "BBB"},
                {"ABCAABCAA", "ABBCAACCBBBBBB"}, {"AAAAZAAAA", "AAAAYAAAA"},
                {"A", "BAB"}, {"AB", "BA"}, {"CAB", "DAB"}})
            check_substring(pair.first, pair.second, unit.weights, unit.penalty);
        const auto tie = common_substring("AB", "BA", unit.weights, 1);
        require(tie.first_start == 0 && tie.second_start == 1 && tie.length == 1, "substring tie order");
        const auto bridge = common_substring("AAAAZAAAA", "AAAAYAAAA", unit.weights, 1);
        require(bridge.score == 7 && bridge.length == 9, "internal mismatch must be allowed");
        Weights zeros{};
        check_substring("AAA", "AAA", zeros, 0);
        rejects([&] { common_substring("a", "A", unit.weights, 1); });
        rejects([&] { common_substring("1", "A", unit.weights, 1); });
        rejects([&] { common_substring("A", "A", unit.weights, -1); });
        Weights invalid = unit.weights; invalid[0] = -1;
        rejects([&] { common_substring("A", "A", invalid, 1); });
        invalid.fill(std::numeric_limits<Score>::max());
        rejects([&] { common_substring("AA", "AA", invalid, 1); });
        rejects([] { scoring(2, 0); });
        rejects([] { scoring(1, 1); });
        std::mt19937 rng(42);
        for (int trial = 0; trial < 2000; ++trial) {
            std::string a(rng() % 9, 'A'), b(rng() % 10, 'A');
            for (char& c : a) c = static_cast<char>('A' + rng() % 4);
            for (char& c : b) c = static_cast<char>('A' + rng() % 4);
            const auto setting = trial % 2 ? scoring(1) : scoring(2, 1 + trial % 10);
            check_substring(a, b, setting.weights, setting.penalty);
            if (trial % 10 == 0) {
                check_substring(a, b, unit.weights, 0);
                check_substring(a, b, unit.weights, std::numeric_limits<Score>::max());
            }
        }
        for (std::size_t m = 0; m <= 3; ++m)
            for (std::size_t n = 0; n <= 3; ++n)
                for (std::uint32_t bits = 0; bits < (1U << (m * n)); ++bits) {
                    std::vector<std::uint8_t> b(m * n);
                    for (std::size_t k = 0; k < b.size(); ++k) b[k] = static_cast<std::uint8_t>((bits >> k) & 1U);
                    check_square(b, m, n);
                }
        for (int trial = 0; trial < 1000; ++trial) {
            const std::size_t m = rng() % 8, n = rng() % 9;
            std::vector<std::uint8_t> b(m * n);
            for (auto& cell : b) cell = static_cast<std::uint8_t>(rng() % 2 ? 7 : 0);
            check_square(b, m, n);
        }
        std::vector<std::uint8_t> large(300 * 310, 0);
        const auto large_answer = largest_zero_square(large, 300, 310);
        require(large_answer.side == 300 && large_answer.row == 0 && large_answer.column == 0,
                "DP side must not be limited to one byte");
        const auto square_tie = largest_zero_square({0,1,0}, 1, 3);
        require(square_tie.column == 0, "square tie order");
        rejects([] { largest_zero_square({0}, 2, 2); });
        rejects([] { largest_zero_square({}, std::numeric_limits<std::size_t>::max(), 2); });
        std::cout << "Passed " << checks << " checks (substring randomized oracle, square exhaustive/random oracle).\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
