// Deterministic mutation test: seeded byte mutations and truncations of valid
// fixtures must never crash and may only fail with std::runtime_error
// (WavError) or OperationCancelled. Complements fuzz/fuzz_wav.cpp.
#include "humtrace/wav.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void u16(std::string& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value & 0xff));
    bytes.push_back(static_cast<char>((value >> 8) & 0xff));
}
void u32(std::string& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<char>((value >> shift) & 0xff));
}
void u64(std::string& bytes, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8)
        bytes.push_back(static_cast<char>((value >> shift) & 0xff));
}
std::string fmt_chunk(std::uint16_t format, std::uint16_t channels, std::uint16_t bits) {
    std::string chunk = "fmt ";
    u32(chunk, 16);
    const auto align = static_cast<std::uint16_t>(channels * (bits / 8));
    u16(chunk, format); u16(chunk, channels); u32(chunk, 48000); u32(chunk, 48000U * align);
    u16(chunk, align); u16(chunk, bits);
    return chunk;
}
std::string data_chunk(const std::string& samples) {
    std::string chunk = "data";
    u32(chunk, static_cast<std::uint32_t>(samples.size()));
    chunk += samples;
    if (samples.size() % 2 != 0) chunk.push_back('\0');
    return chunk;
}
std::string riff(const std::string& body) {
    std::string result = "RIFF";
    u32(result, static_cast<std::uint32_t>(body.size() + 4));
    return result + "WAVE" + body;
}
std::string pattern(std::size_t bytes) {
    std::string s(bytes, '\0');
    for (std::size_t i = 0; i < bytes; ++i) s[i] = static_cast<char>((i * 37 + 11) & 0x7f);
    return s;
}
std::string extensible_fixture() {
    std::string fmt = "fmt ";
    u32(fmt, 40);
    u16(fmt, 0xFFFE); u16(fmt, 2); u32(fmt, 48000); u32(fmt, 48000 * 6); u16(fmt, 6); u16(fmt, 24);
    u16(fmt, 22); u16(fmt, 20); u32(fmt, 0x3);
    u16(fmt, 1); u16(fmt, 0); u16(fmt, 0); u16(fmt, 0x10);
    fmt.append("\x80\x00\x00\xAA\x00\x38\x9B\x71", 8);
    return riff(fmt + data_chunk(pattern(36)));
}
std::string rf64_fixture() {
    const std::string samples = pattern(32);
    std::string list = "LIST";
    u32(list, 0xFFFFFFFFU);
    list += "abcd";
    std::string ds64_body;
    std::string fmt = fmt_chunk(1, 2, 16);
    const std::uint64_t total = 12 + 8 + 28 + 12 + fmt.size() + list.size() + 8 + samples.size();
    u64(ds64_body, total - 8); u64(ds64_body, samples.size()); u64(ds64_body, samples.size() / 4);
    u32(ds64_body, 1);
    ds64_body += "LIST"; u64(ds64_body, 4);
    std::string result = "RF64";
    u32(result, 0xFFFFFFFFU);
    result += "WAVE";
    result += "ds64"; u32(result, static_cast<std::uint32_t>(ds64_body.size())); result += ds64_body;
    result += fmt + list + "data";
    u32(result, 0xFFFFFFFFU);
    return result + samples;
}
std::vector<std::string> fixtures() {
    std::string multi = fmt_chunk(1, 1, 16) + data_chunk(pattern(10)) + "LIST";
    u32(multi, 3);
    multi += "xyz";
    multi.push_back('\0');
    multi += data_chunk(pattern(6));
    return {
        riff(fmt_chunk(1, 1, 16) + data_chunk(pattern(40))),
        riff(fmt_chunk(1, 2, 24) + data_chunk(pattern(30))),
        riff(fmt_chunk(1, 1, 8) + data_chunk(pattern(9))),
        riff(fmt_chunk(3, 1, 32) + data_chunk(std::string("\0\0\x80\x3e\0\0\x80\xbe\0\0\0\0", 12))),
        riff(fmt_chunk(3, 1, 64) + data_chunk(std::string(16, '\0'))),
        riff(multi),
        extensible_fixture(),
        rf64_fixture(),
    };
}

constexpr std::array<std::uint32_t, 10> interesting_u32{
    0, 1, 2, 0x7f, 0x80, 0xffff, 0x7fffffffU, 0x80000000U, 0xfffffffeU, 0xffffffffU};

std::string mutate(std::string bytes, std::mt19937& rng) {
    auto pick = [&](std::size_t n) { return std::uniform_int_distribution<std::size_t>(0, n - 1)(rng); };
    const auto operations = 1 + pick(3);
    for (std::size_t op = 0; op < operations && !bytes.empty(); ++op) {
        switch (pick(7)) {
        case 0: bytes[pick(bytes.size())] ^= static_cast<char>(1U << pick(8)); break;
        case 1: bytes[pick(bytes.size())] = static_cast<char>(pick(256)); break;
        case 2: {  // overwrite an aligned little-endian 32-bit field
            if (bytes.size() < 4) break;
            const auto at = pick(bytes.size() - 3);
            std::string value;
            u32(value, interesting_u32[pick(interesting_u32.size())]);
            bytes.replace(at, 4, value);
            break;
        }
        case 3: bytes.resize(pick(bytes.size())); break;
        case 4: bytes.erase(pick(bytes.size()), 1 + pick(4)); break;
        case 5: bytes.insert(pick(bytes.size()), std::string(1 + pick(4), static_cast<char>(pick(256)))); break;
        default: {  // duplicate a slice
            const auto from = pick(bytes.size());
            const auto length = std::min<std::size_t>(1 + pick(16), bytes.size() - from);
            bytes.insert(pick(bytes.size()), bytes.substr(from, length));
            break;
        }
        }
    }
    return bytes;
}

struct Counts { std::size_t accepted{}, rejected{}, cancelled{}; };

void exercise(const std::string& bytes, std::mt19937& rng, Counts& counts) {
    try {
        std::istringstream stream(bytes, std::ios::binary);
        humtrace::CancellationToken token;
        const bool cancel = rng() % 16 == 0;
        humtrace::WavStreamReader reader(stream, &token);
        if (cancel)
            token.request_cancel();
        const auto frames = reader.metadata().frame_count;
        std::size_t samples = 0;
        while (true) {
            const auto batch = reader.read_frames(1 + rng() % 7);
            if (batch.empty()) break;
            samples += batch.size();
        }
        if (samples != frames * reader.metadata().channels)
            throw std::logic_error("sample count does not match the declared frame count");
        if (frames > 0) {
            reader.seek_frame(rng() % (frames + 1));
            (void)reader.read_frames(4);
        }
        std::istringstream again(bytes, std::ios::binary);
        (void)humtrace::decode_wav(again);
        ++counts.accepted;
    } catch (const humtrace::OperationCancelled&) {
        ++counts.cancelled;
    } catch (const std::runtime_error&) {
        ++counts.rejected;
    }
}
} // namespace

int main() {
    std::mt19937 rng(0x48756d54U);  // fixed seed: deterministic
    Counts counts;
    std::size_t cases = 0;
    try {
        for (const auto& fixture : fixtures()) {
            std::istringstream valid(fixture, std::ios::binary);
            if (humtrace::decode_wav(valid).interleaved_samples.empty()) {
                std::cerr << "FAIL: fixture decodes to no samples\n";
                return 1;
            }
            for (std::size_t length = 0; length <= fixture.size(); ++length, ++cases)
                exercise(fixture.substr(0, length), rng, counts);
            for (int i = 0; i < 600; ++i, ++cases)
                exercise(mutate(fixture, rng), rng, counts);
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception after " << cases << " cases: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "FAIL: non-standard exception after " << cases << " cases\n";
        return 1;
    }
    if (counts.accepted < 8 || counts.rejected < 100) {
        std::cerr << "FAIL: mutation mix is degenerate\n";
        return 1;
    }
    std::cout << "WAV mutation test passed: " << cases << " cases, " << counts.accepted << " accepted, "
              << counts.rejected << " rejected, " << counts.cancelled << " cancelled.\n";
}
