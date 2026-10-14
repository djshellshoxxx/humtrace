#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>

namespace humtrace {

class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t size);
    // Finalize and return lowercase hex digest. The object must not be reused.
    [[nodiscard]] std::string hex_digest();

private:
    void process_block(const unsigned char* block);
    std::array<std::uint32_t, 8> state_;
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffered_{};
    std::uint64_t total_bytes_{};
};

// Stream-hash an input from its current position to EOF in bounded chunks.
[[nodiscard]] std::string sha256_hex(std::istream& input);

} // namespace humtrace
