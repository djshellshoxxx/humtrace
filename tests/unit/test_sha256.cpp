#include "humtrace/sha256.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::string hash(const std::string& message) {
    humtrace::Sha256 hasher;
    hasher.update(message);
    return hasher.hex();
}

// FIPS 180-4 / NIST CAVP known-answer vectors.
const std::string empty_digest = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
const std::string abc_digest = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
const std::string two_block_message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
const std::string two_block_digest = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
const std::string million_a_digest = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";

void nist_vectors() {
    require(hash("") == empty_digest, "empty message");
    require(hash("abc") == abc_digest, "abc");
    require(two_block_message.size() * 8 == 448, "448-bit message length");
    require(hash(two_block_message) == two_block_digest, "448-bit message");
    require(hash(std::string(1000000, 'a')) == million_a_digest, "one million 'a'");
}

void incremental_updates_match_one_shot() {
    // Uneven pieces exercise buffer carry-over across every block offset.
    const std::string message(1000000, 'a');
    humtrace::Sha256 hasher;
    std::size_t offset = 0;
    for (std::size_t piece = 1; offset < message.size(); piece = piece % 131 + 1) {
        const auto take = std::min(piece, message.size() - offset);
        hasher.update(message.data() + offset, take);
        offset += take;
    }
    require(hasher.bytes_processed() == message.size(), "byte count");
    require(hasher.hex() == million_a_digest, "incremental million 'a'");
    // Lengths around the 55/56/64-byte padding boundaries match a byte-at-a-time hash.
    for (std::size_t length = 50; length < 130; ++length) {
        const std::string data(length, 'x');
        humtrace::Sha256 bytewise;
        for (const char c : data) bytewise.update(&c, 1);
        require(bytewise.hex() == hash(data), "padding boundary lengths");
    }
}

void finalize_rules() {
    humtrace::Sha256 hasher;
    hasher.update("abc");
    (void)hasher.finalize();
    bool threw = false;
    try { hasher.update("x"); } catch (const std::logic_error&) { threw = true; }
    require(threw, "update after finalize throws");
    hasher.reset();
    hasher.update("abc");
    require(hasher.hex() == abc_digest, "reset allows reuse");
}

void stream_hash_with_progress() {
    std::istringstream input(std::string(2500000, 'a'));
    std::vector<std::uint64_t> reports;
    const auto result = humtrace::sha256_stream(input, nullptr, [&](std::uint64_t done) { reports.push_back(done); });
    require(result.bytes == 2500000, "stream byte count");
    require(result.hex == hash(std::string(2500000, 'a')), "stream digest");
    require(reports.size() == 3 && reports.back() == 2500000 && reports.front() == humtrace::sha256_block_bytes,
            "progress per bounded block");
    std::istringstream million(std::string(1000000, 'a'));
    require(humtrace::sha256_stream(million).hex == million_a_digest, "stream million 'a'");
    std::istringstream empty;
    require(humtrace::sha256_stream(empty).hex == empty_digest, "empty stream");
}

void stream_hash_cancellation() {
    humtrace::CancellationToken token;
    std::istringstream input(std::string(3000000, 'a'));
    bool cancelled = false;
    try {
        (void)humtrace::sha256_stream(input, &token, [&](std::uint64_t) { token.request_cancel(); });
    } catch (const humtrace::OperationCancelled&) { cancelled = true; }
    require(cancelled, "cancellation is observed between blocks");
}

void file_hash_is_read_only() {
    const auto path = std::filesystem::temp_directory_path() / "humtrace_test_sha256.bin";
    { std::ofstream out(path, std::ios::binary); out << "abc"; }
    const auto before = std::filesystem::last_write_time(path);
    const auto result = humtrace::sha256_file(path);
    require(result.hex == abc_digest && result.bytes == 3, "file digest");
    require(std::filesystem::last_write_time(path) == before && std::filesystem::file_size(path) == 3,
            "file is not modified");
    std::filesystem::remove(path);
    bool threw = false;
    try { (void)humtrace::sha256_file(path); } catch (const std::runtime_error&) { threw = true; }
    require(threw, "missing file is reported");
}
} // namespace

int main() {
    nist_vectors();
    incremental_updates_match_one_shot();
    finalize_rules();
    stream_hash_with_progress();
    stream_hash_cancellation();
    file_hash_is_read_only();
    std::cout << "All SHA-256 tests passed.\n";
}
