#include "humtrace/sha256.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::string hash_of(const std::string& text) {
    std::istringstream input(text);
    return humtrace::digest_stream(input).sha256_hex;
}
} // namespace

int main() {
    // FIPS 180-4 / NIST known-answer vectors.
    require(hash_of("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty");
    require(hash_of("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc");
    require(hash_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "two-block message");
    require(hash_of(std::string(1000000, 'a')) ==
                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "million a");
    std::istringstream sized(std::string(70000, 'x'));
    require(humtrace::digest_stream(sized).bytes == 70000, "byte count crosses buffer boundary");
    std::cout << "All SHA-256 tests passed.\n";
}
