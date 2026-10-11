# Convenience wrapper around the CMake build.
BUILD_DIR ?= build
CMAKE_ARGS ?=

.PHONY: all test clean
all:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_ARGS)
	cmake --build $(BUILD_DIR) --parallel

test: all
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
