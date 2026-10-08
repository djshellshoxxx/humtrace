CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -Werror
CPPFLAGS ?= -Iinclude
BUILD_DIR ?= build
PYTHON ?= python3
CORE_SOURCES = src/spectrum.cpp src/wav.cpp src/report.cpp
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

$(BUILD_DIR)/test_report: src/report.cpp tests/test_report.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/report.cpp tests/test_report.cpp -o $@

test: $(BUILD_DIR)/humtrace $(BUILD_DIR)/test_spectrum $(BUILD_DIR)/test_wav $(BUILD_DIR)/test_report
	$(BUILD_DIR)/test_spectrum
	$(BUILD_DIR)/test_wav
	$(BUILD_DIR)/test_report
	$(PYTHON) tests/test_cli.py $(BUILD_DIR)/humtrace

clean:
	rm -rf $(BUILD_DIR)
