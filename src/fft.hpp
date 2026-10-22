#pragma once

// Internal radix-2 FFT shared by the spectrum and PSD modules. Not part of the public API.

#include <complex>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

namespace humtrace::detail {

// In-place iterative radix-2 transform (unnormalized, forward sign convention).
// The caller guarantees that values.size() is a power of two.
inline void fft_radix2(std::vector<std::complex<double>>& values) {
    const std::size_t size = values.size();
    for (std::size_t i = 1, j = 0; i < size; ++i) {
        std::size_t bit = size >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(values[i], values[j]);
    }

    for (std::size_t length = 2; length <= size; length <<= 1) {
        const auto angle = -2.0 * std::numbers::pi / static_cast<double>(length);
        const std::complex<double> root(std::cos(angle), std::sin(angle));
        for (std::size_t start = 0; start < size; start += length) {
            std::complex<double> factor(1.0, 0.0);
            for (std::size_t offset = 0; offset < length / 2; ++offset) {
                const auto even = values[start + offset];
                const auto odd = values[start + offset + length / 2] * factor;
                values[start + offset] = even + odd;
                values[start + offset + length / 2] = even - odd;
                factor *= root;
            }
        }
    }
}

} // namespace humtrace::detail
