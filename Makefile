CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -Werror
CPPFLAGS ?= -Iinclude
BUILD_DIR ?= build
PYTHON ?= python3
ifeq ($(OS),Windows_NT)
LDLIBS += -lshell32
endif
CORE_SOURCES = $(filter-out src/main.cpp,$(wildcard src/*.cpp))
CORE_HEADERS = $(wildcard include/humtrace/*.hpp src/*.hpp)
TESTS = $(patsubst tests/test_%.cpp,%,$(wildcard tests/test_*.cpp))

.PHONY: all test fuzz clean
all: $(BUILD_DIR)/humtrace

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/humtrace: $(CORE_SOURCES) src/main.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_SOURCES) src/main.cpp -o $@ $(LDLIBS)

# Every C++ unit test links the whole (dependency-free) core.
$(BUILD_DIR)/test_%: tests/test_%.cpp $(CORE_SOURCES) $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_SOURCES) $< -o $@

test: $(BUILD_DIR)/humtrace $(addprefix $(BUILD_DIR)/test_,$(TESTS))
	@set -e; for t in $(TESTS); do $(BUILD_DIR)/test_$$t; done
	$(PYTHON) tests/test_cli.py $(BUILD_DIR)/humtrace

# 30-second randomized settings-matrix fuzz (override: make fuzz FUZZ_SECONDS=60).
FUZZ_SECONDS ?= 30
$(BUILD_DIR)/settings_fuzz: tests/fuzz/settings_fuzz.cpp $(CORE_SOURCES) $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_SOURCES) $< -o $@

fuzz: $(BUILD_DIR)/settings_fuzz
	$(BUILD_DIR)/settings_fuzz $(FUZZ_SECONDS)

clean:
	rm -rf $(BUILD_DIR)
