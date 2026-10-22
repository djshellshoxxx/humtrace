#include "humtrace/sha256.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using humtrace::sha256_hex;

static void check(const std::string& input, const char* expected) {
    std::istringstream stream(input);
    const auto actual = sha256_hex(stream);
    if (actual != expected) {
        std::cerr << "SHA-256 mismatch: " << actual << " != " << expected << '\n';
        std::exit(1);
    }
}

int main() {
    check("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    check(std::string(1000000, 'a'),
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    {
        std::istringstream in(std::string(300000, 'x'));
        if (humtrace::sha256_hex_cancellable(in, [] { return true; }))
            { std::cerr << "cancel ignored\n"; return 1; }
        std::istringstream again(std::string(300000, 'x'));
        std::istringstream plain(std::string(300000, 'x'));
        std::uint64_t seen = 0;
        const auto digest = humtrace::sha256_hex_cancellable(again, {}, [&](std::uint64_t b) { seen = b; });
        if (!digest || *digest != sha256_hex(plain) || seen != 300000)
            { std::cerr << "cancellable hash mismatch\n"; return 1; }
    }
    std::cout << "All SHA-256 tests passed.\n";
}
