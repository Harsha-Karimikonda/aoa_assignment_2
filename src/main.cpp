#include "dp.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sys/resource.h>

using namespace assignment;
std::size_t number(const char* value) {
    const std::string text(value);
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("expected a nonnegative integer");
    const auto parsed = std::stoull(text);
    if (parsed > std::numeric_limits<std::size_t>::max()) throw std::overflow_error("integer too large");
    return static_cast<std::size_t>(parsed);
}
int small_number(const char* value) {
    const auto parsed = number(value);
    if (parsed > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("integer option exceeds int range");
    return static_cast<int>(parsed);
}
std::string random_text(std::size_t n, std::mt19937& rng) {
    std::uniform_int_distribution<int> uniform(0, 25);
    std::string result(n, 'A');
    for (char& c : result) c = static_cast<char>('A' + uniform(rng));
    return result;
}
std::vector<std::uint8_t> random_matrix(std::size_t m, std::size_t n, std::mt19937& rng) {
    if (n && m > std::numeric_limits<std::size_t>::max() / n)
        throw std::overflow_error("matrix dimensions overflow");
    std::vector<std::uint8_t> result(m * n);
    std::bernoulli_distribution one(0.5);
    for (auto& value : result) value = static_cast<std::uint8_t>(one(rng));
    return result;
}
std::uint64_t peak_rss_bytes() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) throw std::runtime_error("getrusage failed");
#ifdef __APPLE__
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
}
volatile std::uint64_t sink = 0;
template<class Function> std::pair<double, std::size_t> measure(Function solve) {
    sink = sink ^ solve(); // warm-up, outside timing
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    std::size_t calls = 0;
    double seconds = 0;
    do {
        sink = sink ^ solve();
        ++calls;
        seconds = std::chrono::duration<double>(Clock::now() - start).count();
    } while (seconds < 0.1);
    return {seconds * 1e9 / static_cast<double>(calls), calls};
}
void benchmark(int argc, char** argv) {
    // bench substring m n scenario step, or bench matrix m n
    if (argc < 5) throw std::invalid_argument("incomplete benchmark arguments");
    const std::string problem(argv[2]);
    const auto m = number(argv[3]), n = number(argv[4]);
    const bool substring = problem == "substring";
    if ((!substring && problem != "matrix") || (substring ? argc != 7 : argc != 5))
        throw std::invalid_argument("invalid benchmark arguments");
    const int scenario = substring ? small_number(argv[5]) : 0;
    const int step = substring ? small_number(argv[6]) : 0;
    const auto settings = substring ? scoring(scenario, step) : Scoring{};
    std::cout << "problem,m,n,scenario,penalty_step,penalty,seed,calls,ns_per_call,input_bytes,dp_bytes,result_char_bytes,peak_rss_bytes,score,length,first_row,second_column\n";
    std::cout << std::setprecision(15);
    for (std::uint32_t seed = 20261102; seed < 20261112; ++seed) {
        std::mt19937 rng(seed);
        std::pair<double, std::size_t> timing;
        double score = 0;
        std::size_t length = 0, first = 0, second = 0, output = 0;
        const std::size_t input = substring ? m + n : m * n;
        const std::size_t dp = substring ? 2 * (n + 1) * (sizeof(Score) + sizeof(std::size_t))
                                       : (n + 1) * sizeof(std::uint32_t);
        if (substring) {
            const auto a = random_text(m, rng);
            const auto b = random_text(n, rng);
            const auto answer = common_substring(a, b, settings.weights, settings.penalty);
            score = static_cast<double>(answer.score) / settings.scale;
            length = answer.length; first = answer.first_start; second = answer.second_start;
            output = 2 * length;
            timing = measure([&] {
                const auto answer2 = common_substring(a, b, settings.weights, settings.penalty);
                return static_cast<std::uint64_t>(answer2.score) + answer2.length;
            });
        } else {
            const auto matrix = random_matrix(m, n, rng);
            const auto answer = largest_zero_square(matrix, m, n);
            length = answer.side; first = answer.row; second = answer.column;
            timing = measure([&] { return largest_zero_square(matrix, m, n).side; });
        }
        std::cout << problem << ',' << m << ',' << n << ',' << scenario
                  << ',' << step << ',' << (substring ? static_cast<double>(settings.penalty) / settings.scale : 0)
                  << ',' << seed << ',' << timing.second << ',' << timing.first << ',' << input
                  << ',' << dp << ',' << output << ',' << peak_rss_bytes() << ',' << score
                  << ',' << length << ',' << first << ',' << second << '\n';
    }
}
int main(int argc, char** argv) {
    try {
        if (argc >= 2 && std::string(argv[1]) == "bench") { benchmark(argc, argv); return 0; }
        if (argc >= 2 && std::string(argv[1]) == "substring") {
            if (argc != 5 && argc != 6) throw std::invalid_argument("substring needs X Y scenario [step]");
            const auto settings = scoring(small_number(argv[4]), argc == 6 ? small_number(argv[5]) : 0);
            const auto result = common_substring(argv[2], argv[3], settings.weights, settings.penalty);
            std::cout << std::setprecision(12) << "score=" << static_cast<double>(result.score) / settings.scale
                      << " first_start=" << result.first_start << " second_start=" << result.second_start
                      << " length=" << result.length << '\n' << result.first << '\n' << result.second << '\n';
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "matrix") {
            std::string m_text, n_text;
            if (!(std::cin >> m_text >> n_text)) throw std::invalid_argument("expected rows columns then cells");
            const auto m = number(m_text.c_str()), n = number(n_text.c_str());
            if (n && m > std::numeric_limits<std::size_t>::max() / n) throw std::overflow_error("dimensions overflow");
            std::vector<std::uint8_t> matrix(m * n);
            for (auto& cell : matrix) {
                long long value;
                if (!(std::cin >> value)) throw std::invalid_argument("missing or invalid matrix cell");
                cell = static_cast<std::uint8_t>(value != 0);
            }
            std::string extra;
            if (std::cin >> extra) throw std::invalid_argument("extra matrix input");
            const auto result = largest_zero_square(matrix, m, n);
            std::cout << "row=" << result.row << " column=" << result.column << " side=" << result.side
                      << " area=" << result.side * result.side << '\n';
            for (std::size_t i = 0; i < result.side; ++i) {
                for (std::size_t j = 0; j < result.side; ++j)
                    std::cout << (j ? " " : "") << static_cast<int>(matrix[(result.row + i) * n + result.column + j]);
                std::cout << '\n';
            }
            return 0;
        }
        std::cerr << "Usage:\n  dp substring X Y 1\n  dp substring X Y 2 STEP\n  dp matrix < matrix.txt\n"
                     "  dp bench substring M N SCENARIO STEP\n  dp bench matrix M N\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n'; return 1;
    }
}
