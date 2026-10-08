CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -Werror
CPPFLAGS ?= -Iinclude
BUILD_DIR ?= build
CORE_SOURCES = src/spectrum.cpp src/wav.cpp

.PHONY: all test clean
all: $(BUILD_DIR)/humtrace

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/humtrace: $(CORE_SOURCES) src/main.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/test_spectrum: src/spectrum.cpp tests/test_spectrum.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/test_wav: src/wav.cpp tests/test_wav.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

test: $(BUILD_DIR)/test_spectrum $(BUILD_DIR)/test_wav
	$(BUILD_DIR)/test_spectrum
	$(BUILD_DIR)/test_wav

clean:
	rm -rf $(BUILD_DIR)
