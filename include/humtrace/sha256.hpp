#pragma once

#include "humtrace/cancellation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <string>
#include <string_view>

namespace humtrace {

// SHA-256 (FIPS 180-4), dependency-free.
using Sha256Digest = std::array<std::uint8_t, 32>;

class Sha256 {
public:
    Sha256() noexcept { reset(); }
    void reset() noexcept;
    // Throws std::logic_error after finalize() until reset().
    void update(const void* data, std::size_t size);
    void update(std::string_view bytes) { update(bytes.data(), bytes.size()); }
    // Pads and returns the digest; the object must be reset() before reuse.
    [[nodiscard]] Sha256Digest finalize();
    // finalize() rendered as 64 lowercase hex characters.
    [[nodiscard]] std::string hex();
    [[nodiscard]] std::uint64_t bytes_processed() const noexcept { return length_; }

private:
    void compress(const std::uint8_t* block) noexcept;
    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffered_{};
    std::uint64_t length_{};
    bool finalized_{};
};

[[nodiscard]] std::string to_hex(const Sha256Digest& digest);

struct Sha256Result {
    Sha256Digest digest{};
    std::string hex;
    std::uint64_t bytes{};
};

// Called after each block with the cumulative number of bytes hashed.
using Sha256Progress = std::function<void(std::uint64_t bytes_done)>;

// Bytes read per block by sha256_stream()/sha256_file().
inline constexpr std::size_t sha256_block_bytes = 1 << 20;

// Hashes from the stream's current position to end of stream in bounded
// blocks. Polls cancel before each block (throws OperationCancelled); throws
// std::runtime_error on a read error.
[[nodiscard]] Sha256Result sha256_stream(std::istream& input, const CancellationToken* cancel = nullptr,
                                         const Sha256Progress& progress = {});

// Opens path read-only and hashes every byte. Throws std::runtime_error if the
// file cannot be opened, or if its size changes while it is being hashed.
[[nodiscard]] Sha256Result sha256_file(const std::filesystem::path& path, const CancellationToken* cancel = nullptr,
                                       const Sha256Progress& progress = {});

} // namespace humtrace
