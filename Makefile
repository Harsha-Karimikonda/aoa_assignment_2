CXX = c++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Wpedantic -Wconversion

.PHONY: all test experiments clean
all: build/dp
build:
	mkdir -p build
build/dp: src/main.cpp src/dp.hpp | build
	$(CXX) $(CXXFLAGS) src/main.cpp -o $@
build/test_dp: tests/test_dp.cpp src/dp.hpp | build
	$(CXX) $(CXXFLAGS) tests/test_dp.cpp -o $@
build/experiments: src/experiments.cpp | build
	$(CXX) $(CXXFLAGS) src/experiments.cpp -o $@
test: build/test_dp
	./build/test_dp
experiments: all build/experiments
	./build/experiments
clean:
	rm -rf build
