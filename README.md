# Dynamic Programming Assignment 2

C++17 solutions for weighted approximate common substrings and the largest all-zero square. Both algorithms and solution extraction use iterative loops and contiguous arrays.

[REPORT.md](REPORT.md) contains the optimization functions, Bellman equations, correctness proofs, extraction procedures, complexity analysis, and experimental results.

## Build and run

```sh
make
./build/dp substring ABCAABCAA ABBCAACCBBBBBB 1
./build/dp substring ABCAABCAA ABBCAACCBBBBBB 2 5
printf '2 3\n0 0 1\n0 0 1\n' | ./build/dp matrix
```

For substrings, use scenario `1`, or scenario `2` followed by penalty step `1` through `10`. Output includes score, zero-based starts, length, and both extracted substrings. Inputs use uppercase A–Z and no gaps. Scenario 1 assumes match +1 and mismatch −1 because the assignment's pasted formulas were missing.

Matrix input gives rows, columns, then the cells. Nonzero values become one; each cell is stored in one byte. Output includes zero-based top-left coordinates, side length, area, and the extracted square.

## Experiments

Data generation, measurements, CSV summaries, and graph generation are all implemented in C++17. No third-party libraries are required. The experiment runner uses POSIX process and memory APIs on macOS/Linux.

```sh
make experiments
```

This regenerates `results/raw.csv`, `results/summary.csv`, and three SVG graphs in `figures/`. It runs both substring scoring scenarios, including ten intermediate penalties for Scenario 2, and all five required matrix sizes. Repeated measurements take about a minute. The report's tables describe the submitted run; update them if replacing the measurements. SVG graphs can be viewed in a browser or the Markdown report.

To regenerate summaries and graphs from the saved measurements:

```sh
make build/experiments
./build/experiments --plots-only
```

To check correctness against exhaustive solutions on small inputs:

```sh
make test
```

The DP implementations are in `src/dp.hpp`; input handling, synthetic generators, and measurements are in `src/main.cpp`. The C++ experiment runner in `src/experiments.cpp` summarizes the CSV data and writes SVG graphs directly. Timing includes solver allocation and substring extraction but excludes generation and output. Memory results distinguish whole-process peak RSS from calculated input and DP array storage. Peak RSS uses `getrusage` on macOS/Linux.
