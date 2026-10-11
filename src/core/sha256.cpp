#include "humtrace/sha256.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace humtrace {
namespace {
constexpr std::array<std::uint32_t, 64> round_constants{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

[[nodiscard]] constexpr std::uint32_t load_be32(const std::uint8_t* bytes) noexcept {
    return (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) | (std::uint32_t{bytes[2]} << 8) |
           std::uint32_t{bytes[3]};
}
} // namespace

void Sha256::reset() noexcept {
    state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    buffered_ = 0;
    length_ = 0;
    finalized_ = false;
}

void Sha256::compress(const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t t = 0; t < 16; ++t)
        w[t] = load_be32(block + 4 * t);
    for (std::size_t t = 16; t < 64; ++t) {
        const auto s0 = std::rotr(w[t - 15], 7) ^ std::rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
        const auto s1 = std::rotr(w[t - 2], 17) ^ std::rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
        w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    auto [a, b, c, d, e, f, g, h] = state_;
    for (std::size_t t = 0; t < 64; ++t) {
        const auto big_s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const auto choose = (e & f) ^ (~e & g);
        const auto temp1 = h + big_s1 + choose + round_constants[t] + w[t];
        const auto big_s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const auto majority = (a & b) ^ (a & c) ^ (b & c);
        const auto temp2 = big_s0 + majority;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void Sha256::update(const void* data, std::size_t size) {
    if (finalized_)
        throw std::logic_error("Sha256::update called after finalize");
    // The message length is encoded in bits as a 64-bit value.
    constexpr std::uint64_t max_bytes = std::numeric_limits<std::uint64_t>::max() / 8;
    if (static_cast<std::uint64_t>(size) > max_bytes - length_)
        throw std::length_error("SHA-256 message exceeds 2^64 bits");
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    length_ += size;
    if (buffered_ > 0) {
        const auto take = std::min(size, buffer_.size() - buffered_);
        std::memcpy(buffer_.data() + buffered_, bytes, take);
        buffered_ += take;
        bytes += take;
        size -= take;
        if (buffered_ < buffer_.size())
            return;
        compress(buffer_.data());
        buffered_ = 0;
    }
    while (size >= buffer_.size()) {
        compress(bytes);
        bytes += buffer_.size();
        size -= buffer_.size();
    }
    if (size > 0) {
        std::memcpy(buffer_.data(), bytes, size);
        buffered_ = size;
    }
}

Sha256Digest Sha256::finalize() {
    if (finalized_)
        throw std::logic_error("Sha256::finalize called twice");
    const std::uint64_t bit_length = length_ * 8;
    buffer_[buffered_++] = 0x80;
    if (buffered_ > 56) {
        std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.end(), std::uint8_t{0});
        compress(buffer_.data());
        buffered_ = 0;
    }
    std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.begin() + 56, std::uint8_t{0});
    for (std::size_t byte = 0; byte < 8; ++byte)
        buffer_[56 + byte] = static_cast<std::uint8_t>(bit_length >> (56 - 8 * byte));
    compress(buffer_.data());
    finalized_ = true;
    Sha256Digest digest{};
    for (std::size_t word = 0; word < 8; ++word)
        for (std::size_t byte = 0; byte < 4; ++byte)
            digest[4 * word + byte] = static_cast<std::uint8_t>(state_[word] >> (24 - 8 * byte));
    return digest;
}

std::string Sha256::hex() { return to_hex(finalize()); }

std::string to_hex(const Sha256Digest& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(digest.size() * 2);
    for (const auto byte : digest) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 0x0f]);
    }
    return text;
}

Sha256Result sha256_stream(std::istream& input, const CancellationToken* cancel, const Sha256Progress& progress) {
    Sha256 hasher;
    std::vector<char> block(sha256_block_bytes);
    while (true) {
        throw_if_cancelled(cancel);
        input.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto got = input.gcount();
        if (input.bad())
            throw std::runtime_error("SHA-256: read error while hashing input");
        if (got > 0) {
            hasher.update(block.data(), static_cast<std::size_t>(got));
            if (progress)
                progress(hasher.bytes_processed());
        }
        if (input.eof())
            break;
        if (!input)
            throw std::runtime_error("SHA-256: read error while hashing input");
    }
    Sha256Result result;
    result.bytes = hasher.bytes_processed();
    result.digest = hasher.finalize();
    result.hex = to_hex(result.digest);
    return result;
}

Sha256Result sha256_file(const std::filesystem::path& path, const CancellationToken* cancel,
                         const Sha256Progress& progress) {
    std::error_code error;
    const auto size_before = std::filesystem::file_size(path, error);
    if (error)
        throw std::runtime_error("SHA-256: cannot determine input file size");
    std::ifstream input(path, std::ios::binary | std::ios::in);
    if (!input)
        throw std::runtime_error("SHA-256: cannot open input file for reading");
    auto result = sha256_stream(input, cancel, progress);
    const auto size_after = std::filesystem::file_size(path, error);
    if (error || result.bytes != size_before || size_after != size_before)
        throw std::runtime_error("SHA-256: input file changed size while it was being hashed");
    return result;
}

} // namespace humtrace
