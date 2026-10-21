#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>

namespace humtrace {

// Streaming SHA-256 (FIPS 180-4). The hash identifies a byte sequence only; it
// does not establish source, authorship, acquisition time, or custody.
class Sha256 {
public:
    Sha256() noexcept;
    void update(const void* data, std::size_t size) noexcept;
    // Finalize and return the lowercase hex digest. The object must not be reused.
    [[nodiscard]] std::string hex_digest();

private:
    void process_block(const unsigned char* block) noexcept;
    std::array<std::uint32_t, 8> state_{};
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffered_{};
    std::uint64_t total_bytes_{};
};

struct FileDigest {
    std::string sha256_hex;
    std::uint64_t bytes{};
};

// Hash a stream to EOF. Throws std::runtime_error on a read failure.
[[nodiscard]] FileDigest digest_stream(std::istream& input);

} // namespace humtrace
