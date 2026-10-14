CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -Werror
CPPFLAGS ?= -Iinclude
BUILD_DIR ?= build
PYTHON ?= python3
CORE_SOURCES = src/spectrum.cpp src/wav.cpp src/report.cpp src/tracker.cpp src/sha256.cpp src/pipeline.cpp
CORE_HEADERS = $(wildcard include/humtrace/*.hpp)

.PHONY: all test clean
all: $(BUILD_DIR)/humtrace

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/humtrace: $(CORE_SOURCES) src/main.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_SOURCES) src/main.cpp -o $@

$(BUILD_DIR)/test_spectrum: src/spectrum.cpp tests/test_spectrum.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/spectrum.cpp tests/test_spectrum.cpp -o $@

$(BUILD_DIR)/test_wav: src/wav.cpp tests/test_wav.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/wav.cpp tests/test_wav.cpp -o $@

$(BUILD_DIR)/test_report: src/report.cpp src/tracker.cpp tests/test_report.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/report.cpp src/tracker.cpp tests/test_report.cpp -o $@

$(BUILD_DIR)/test_tracker: src/tracker.cpp tests/test_tracker.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/tracker.cpp tests/test_tracker.cpp -o $@

$(BUILD_DIR)/test_sha256: src/sha256.cpp tests/test_sha256.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/sha256.cpp tests/test_sha256.cpp -o $@

test: $(BUILD_DIR)/humtrace $(BUILD_DIR)/test_spectrum $(BUILD_DIR)/test_wav $(BUILD_DIR)/test_report $(BUILD_DIR)/test_tracker $(BUILD_DIR)/test_sha256
	$(BUILD_DIR)/test_tracker
	$(BUILD_DIR)/test_sha256
	$(BUILD_DIR)/test_spectrum
	$(BUILD_DIR)/test_wav
	$(BUILD_DIR)/test_report
	$(PYTHON) tests/test_cli.py $(BUILD_DIR)/humtrace

clean:
	rm -rf $(BUILD_DIR)
