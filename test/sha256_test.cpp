// Unit tests for the FIPS 180-4 SHA-256 implementation that backs the
// versioned identity derivation (#10). The vectors are the published
// NIST/FIPS known answers plus boundary lengths (55/56/57 and 64 bytes) that
// exercise both sides of the padding rule; the million-'a' case covers many
// full blocks. If this ever changes, every derived world seed does too.

#include <iostream>
#include <string>

#include "util/sha256.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at sha256_test.cpp:" << __LINE__ << "\n";           \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

struct Vector {
    std::string input;
    const char *hex;
};

// Lowercase hex of the 32-byte digest.
std::string hexDigest(const std::string &input) {
    unsigned char digest[keepsake::util::SHA256_DIGEST_SIZE];
    keepsake::util::sha256(
        reinterpret_cast<const unsigned char *>(input.data()), input.size(),
        digest);
    static const char *digits = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (unsigned char byte : digest) {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

} // namespace

int main() {
    using namespace keepsake::util;

    // Published known answers, including the empty string and the canonical
    // two-block message.
    const Vector vectors[] = {
        {"",
         "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc",
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
        {std::string(55, 'a'),
         "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {std::string(56, 'a'),
         "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
        {std::string(57, 'a'),
         "f13b2d724659eb3bf47f2dd6af1accc87b81f09f59f2b75e5c0bed6589dfe8c6"},
        {std::string(63, 'a'),
         "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
        {std::string(64, 'a'),
         "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
        {std::string(65, 'a'),
         "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
    };

    for (const Vector &vector : vectors) {
        const std::string got = hexDigest(vector.input);
        if (got != vector.hex) {
            std::cerr << "FAILED: SHA-256 of " << vector.input.size()
                      << "-byte input\nexpected " << vector.hex << "\ngot      "
                      << got << "\n";
            ++g_failures;
        }
        CHECK(got.size() == 64);
    }

    // A long multi-block input (1,000,000 bytes is exactly 15,625 blocks,
    // so the final block is padding only).
    CHECK(sha256Hex(std::string(1000000, 'a')) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    // The version comes first as an 8-byte big-endian integer: version 1 and
    // version 2 of the same DID must never collide, and the documented
    // fixtures must match the independent hashlib computation used in CI.
    CHECK(deriveSha256(1, "did:plc:testaccount1234", "world") ==
          "6aca338882d30b069c6ff2264f21c592de4e6904a9038565286d783ecdd803a8");
    CHECK(deriveSha256(2, "did:plc:testaccount1234", "world") !=
          deriveSha256(1, "did:plc:testaccount1234", "world"));
    CHECK(deriveSha256(1, "did:web:example.com", "world") ==
          "f5d6fe9da4ebab9ee4a334af52a57eec20126d2b91b2ce4350292746011fafb3");

    std::cout << "sha256_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
