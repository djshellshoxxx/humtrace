// Purpose: streaming SHA-256 (FIPS 180-4) for input-integrity metadata.
// Original implementation written for HumTrace; no third-party code.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace humtrace {

class Sha256 {
public:
    Sha256() noexcept;
    void update(const unsigned char* data, std::size_t size) noexcept;
    // Finalizes and returns lowercase hex. The object must not be reused.
    [[nodiscard]] std::string hex_digest();

private:
    void process_block(const unsigned char* block) noexcept;
    std::array<std::uint32_t, 8> state_;
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffered_{};
    std::uint64_t total_bytes_{};
};

} // namespace humtrace
