#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <spawn.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

struct Summary {
    std::string problem;
    std::size_t m, n;
    int scenario, step;
    double penalty, milliseconds, score, length;
    std::uint64_t input_bytes, dp_bytes, rss_bytes;
};

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) fields.push_back(field);
    return fields;
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
}

// Each benchmark has its own process so RSS from an earlier configuration
// cannot become the high-water mark of a later configuration.
void run_configuration(std::vector<std::string> arguments) {
    arguments.insert(arguments.begin(), {"./build/dp", "bench"});
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0)
        throw std::runtime_error("cannot initialize benchmark process");
    const int open_error = posix_spawn_file_actions_addopen(
        &actions, STDOUT_FILENO, "build/benchmark.csv", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    pid_t child = 0;
    const int spawn_error = open_error ? open_error
        : posix_spawn(&child, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawn_error != 0) throw std::runtime_error("cannot start build/dp benchmark");
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited == -1 && errno == EINTR);
    if (waited == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        throw std::runtime_error("benchmark process failed");
}

void run_experiments() {
    std::filesystem::create_directories("results");
    std::ofstream raw("results/raw.csv");
    if (!raw) throw std::runtime_error("cannot write results/raw.csv");
    const std::vector<std::pair<int, int>> strings = {
        {100, 100}, {100, 1000}, {500, 500}, {1000, 1000}, {2000, 2000}};
    const std::vector<std::pair<int, int>> matrices = {
        {10, 10}, {10, 100}, {10, 1000}, {100, 1000}, {1000, 1000}};
    std::vector<std::vector<std::string>> configurations;
    for (auto [m, n] : strings) {
        configurations.push_back({"substring", std::to_string(m), std::to_string(n), "1", "0"});
        for (int step = 1; step <= 10; ++step)
            configurations.push_back({"substring", std::to_string(m), std::to_string(n), "2", std::to_string(step)});
    }
    for (auto [m, n] : matrices)
        configurations.push_back({"matrix", std::to_string(m), std::to_string(n)});
    for (std::size_t i = 0; i < configurations.size(); ++i) {
        std::cout << '[' << i + 1 << '/' << configurations.size() << "] ";
        for (const auto& argument : configurations[i]) std::cout << argument << ' ';
        std::cout << std::endl;
        run_configuration(configurations[i]);
        std::ifstream batch("build/benchmark.csv");
        std::string line;
        if (!std::getline(batch, line)) throw std::runtime_error("missing benchmark header");
        if (i == 0) raw << line << '\n';
        int count = 0;
        while (std::getline(batch, line)) { raw << line << '\n'; ++count; }
        if (count != 10) throw std::runtime_error("expected ten seeds per configuration");
        raw.flush();
    }
    if (!raw) throw std::runtime_error("failed writing raw measurements");
}

std::vector<Summary> summarize() {
    std::ifstream raw("results/raw.csv");
    std::string line;
    if (!std::getline(raw, line)) throw std::runtime_error("cannot read results/raw.csv");
    std::vector<Summary> result;
    std::vector<double> times, scores, lengths;
    std::uint64_t rss = 0;
    std::vector<std::string> first;
    auto finish = [&] {
        if (first.empty()) return;
        if (times.size() != 10) throw std::runtime_error("incomplete measurement group");
        result.push_back({first[0], std::stoull(first[1]), std::stoull(first[2]),
            std::stoi(first[3]), std::stoi(first[4]), std::stod(first[5]),
            median(times) / 1e6, median(scores), median(lengths),
            std::stoull(first[9]), std::stoull(first[10]), rss});
        times.clear(); scores.clear(); lengths.clear(); rss = 0;
    };
    while (std::getline(raw, line)) {
        const auto fields = split(line);
        if (fields.size() != 17) throw std::runtime_error("invalid measurement CSV row");
        if (!first.empty() && !std::equal(fields.begin(), fields.begin() + 5, first.begin())) {
            finish(); first.clear();
        }
        if (first.empty()) first = fields;
        times.push_back(std::stod(fields[8]));
        scores.push_back(std::stod(fields[13]));
        lengths.push_back(std::stod(fields[14]));
        rss = std::max(rss, static_cast<std::uint64_t>(std::stoull(fields[12])));
    }
    finish();
    if (result.size() != 60) throw std::runtime_error("expected 60 experiment configurations");
    std::ofstream output("results/summary.csv");
    output << std::setprecision(15)
           << "problem,m,n,scenario,penalty_step,penalty,median_ms,input_bytes,dp_bytes,peak_rss_bytes,median_score,median_length\n";
    for (const auto& row : result)
        output << row.problem << ',' << row.m << ',' << row.n << ',' << row.scenario << ',' << row.step
               << ',' << row.penalty << ',' << row.milliseconds << ',' << row.input_bytes << ',' << row.dp_bytes
               << ',' << row.rss_bytes << ',' << row.score << ',' << row.length << '\n';
    if (!output) throw std::runtime_error("cannot write results/summary.csv");
    return result;
}

std::string label(double value) {
    std::ostringstream stream;
    stream << std::setprecision(4) << value;
    return stream.str();
}

// SVG is an image format: this writes the graphs directly from C++ without
// a plotting package or an external program.
void graph(const std::string& filename, const std::string& title,
           const std::string& x_title, const std::string& y_title,
           const std::vector<std::string>& labels, const std::vector<double>& values,
           bool logarithmic = false) {
    if (values.size() < 2 || values.size() != labels.size())
        throw std::runtime_error("invalid graph data");
    const double low = logarithmic ? std::floor(std::log10(*std::min_element(values.begin(), values.end()))) : 0;
    const double high = logarithmic ? std::ceil(std::log10(*std::max_element(values.begin(), values.end())))
                                   : *std::max_element(values.begin(), values.end()) * 1.15;
    auto x = [&](std::size_t i) { return 130 + 680.0 * static_cast<double>(i) / static_cast<double>(values.size() - 1); };
    auto y = [&](double v) { return 370 - 290 * ((logarithmic ? std::log10(v) : v) - low) / (high - low); };
    std::ofstream svg(filename);
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"900\" height=\"470\" viewBox=\"0 0 900 470\">\n"
        << "<rect width=\"900\" height=\"470\" fill=\"white\"/>\n"
        << "<g font-family=\"Arial, sans-serif\" font-size=\"14\" fill=\"#222\">\n"
        << "<text x=\"450\" y=\"32\" text-anchor=\"middle\" font-size=\"21\">" << title << "</text>\n";
    for (int tick = 0; tick <= 5; ++tick) {
        const double level = low + (high - low) * tick / 5;
        const double position = 370 - 290.0 * tick / 5;
        svg << "<line x1=\"130\" x2=\"810\" y1=\"" << position << "\" y2=\"" << position << "\" stroke=\"#ddd\"/>\n"
            << "<text x=\"120\" y=\"" << position + 5 << "\" text-anchor=\"end\">"
            << label(logarithmic ? std::pow(10, level) : level) << "</text>\n";
    }
    svg << "<path d=\"M130 80 V370 H810\" fill=\"none\" stroke=\"#222\"/>\n"
        << "<polyline fill=\"none\" stroke=\"#2368ad\" stroke-width=\"3\" points=\"";
    for (std::size_t i = 0; i < values.size(); ++i) svg << x(i) << ',' << y(values[i]) << ' ';
    svg << "\"/>\n";
    for (std::size_t i = 0; i < values.size(); ++i)
        svg << "<circle cx=\"" << x(i) << "\" cy=\"" << y(values[i]) << "\" r=\"5\" fill=\"#2368ad\"/>\n"
            << "<text x=\"" << x(i) << "\" y=\"397\" text-anchor=\"middle\">" << labels[i] << "</text>\n";
    svg << "<text x=\"450\" y=\"442\" text-anchor=\"middle\">" << x_title << "</text>\n"
        << "<text transform=\"translate(25 225) rotate(-90)\" text-anchor=\"middle\">" << y_title << "</text>\n"
        << "</g>\n</svg>\n";
    if (!svg) throw std::runtime_error("cannot write graph " + filename);
}

void plot(const std::vector<Summary>& rows) {
    std::filesystem::create_directories("figures");
    std::vector<std::string> penalties, dimensions;
    std::vector<double> scores, times, memory;
    for (const auto& row : rows) {
        if (row.problem == "substring" && row.scenario == 2 && row.m == 2000) {
            penalties.push_back(label(row.penalty)); scores.push_back(row.score);
        } else if (row.problem == "matrix") {
            dimensions.push_back(std::to_string(row.m) + "x" + std::to_string(row.n));
            times.push_back(row.milliseconds);
            memory.push_back(static_cast<double>(row.rss_bytes) / (1024 * 1024));
        }
    }
    graph("figures/substring_penalties.svg", "Weighted substring: Scenario 2 (2000 x 2000)",
          "Mismatch penalty magnitude", "Median optimum score", penalties, scores);
    graph("figures/matrix_runtime.svg", "Largest zero square: running time",
          "Matrix dimensions", "Median runtime (ms, logarithmic scale)", dimensions, times, true);
    graph("figures/matrix_memory.svg", "Largest zero square: memory usage",
          "Matrix dimensions", "Peak process RSS (MiB)", dimensions, memory);
}

int main(int argc, char** argv) {
    try {
        if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--plots-only"))
            throw std::invalid_argument("usage: build/experiments [--plots-only]");
        if (argc == 1) run_experiments();
        plot(summarize());
        std::cout << "Saved summary.csv and three SVG graphs.\n";
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n'; return 1;
    }
}
