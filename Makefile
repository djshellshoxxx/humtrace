CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -Werror
CPPFLAGS ?= -Iinclude
BUILD_DIR ?= build
PYTHON ?= python3
ifeq ($(OS),Windows_NT)
LDLIBS += -lshell32
endif
CORE_SOURCES = $(filter-out src/main.cpp,$(wildcard src/*.cpp))
CORE_OBJECTS = $(patsubst src/%.cpp,$(BUILD_DIR)/core/%.o,$(CORE_SOURCES))
CORE_HEADERS = $(wildcard include/humtrace/*.hpp src/*.hpp)
TESTS = $(patsubst tests/test_%.cpp,%,$(wildcard tests/test_*.cpp))

.PHONY: all test fuzz clean
all: $(BUILD_DIR)/humtrace

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/humtrace: $(CORE_OBJECTS) src/main.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_OBJECTS) src/main.cpp -o $@ $(LDLIBS)

# Compile the core once instead of rebuilding it for every test executable.
$(BUILD_DIR)/core/%.o: src/%.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(CORE_OBJECTS:.o=.d)

# Every C++ unit test links the whole (dependency-free) core.
$(BUILD_DIR)/test_%: tests/test_%.cpp $(CORE_OBJECTS) $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_OBJECTS) $< -o $@

test: $(BUILD_DIR)/humtrace $(addprefix $(BUILD_DIR)/test_,$(TESTS))
	@set -e; for t in $(TESTS); do $(BUILD_DIR)/test_$$t; done
	$(PYTHON) tests/test_cli.py $(BUILD_DIR)/humtrace
	$(PYTHON) tests/test_streaming.py $(BUILD_DIR)/humtrace
	$(PYTHON) tests/test_cli_paging.py $(BUILD_DIR)/humtrace
	$(PYTHON) tests/test_detector_cli.py $(BUILD_DIR)/humtrace
	$(PYTHON) tests/test_packaging.py
	$(PYTHON) tests/test_package_licenses.py
	$(PYTHON) tests/test_qt_sources.py
	$(PYTHON) tests/test_macos_packaging.py
	$(PYTHON) tests/test_package_source.py

# 30-second randomized settings-matrix fuzz (override: make fuzz FUZZ_SECONDS=60).
FUZZ_SECONDS ?= 30
$(BUILD_DIR)/settings_fuzz: tests/fuzz/settings_fuzz.cpp $(CORE_OBJECTS) $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_OBJECTS) $< -o $@

fuzz: $(BUILD_DIR)/settings_fuzz
	$(BUILD_DIR)/settings_fuzz $(FUZZ_SECONDS)

clean:
	rm -rf $(BUILD_DIR)
