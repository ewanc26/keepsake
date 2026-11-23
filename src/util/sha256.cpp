#include "util/sha256.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace keepsake::util {

namespace {

// Initial hash values (first 32 bits of fractional parts of square roots of
// first 8 primes 2..19).
const uint32_t H0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

// Round constants (first 32 bits of fractional parts of cube roots of
// first 64 primes 2..311).
const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr32(uint32_t x, int n) {
    n %= 32;
    if (n == 0) return x;
    return (x >> n) | (x << (32 - n));
}

inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

// One SHA-256 compression block: state <- transform(state, block)
void sha256Transform(const unsigned char block[64], uint32_t state[8]) {
    // Load W[0..15] from the block, big-endian.
    uint32_t W[64];
    for (int i = 0; i < 16; ++i) {
        uint32_t v = 0;
        for (int b = 0; b < 4; ++b) {
            v = (v << 8) | (uint32_t)block[(size_t)i * 4 + b];
        }
        W[i] = v;
    }

    for (int i = 16; i < 64; ++i) {
        uint32_t w15 = W[i - 15];
        uint32_t w2 = W[i - 2];
        uint32_t s0 = rotr32(w15, 7) ^ rotr32(w15, 18) ^ (w15 >> 3);
        uint32_t s1 = rotr32(w2, 17) ^ rotr32(w2, 19) ^ (w2 >> 10);
        W[i] = W[i - 16] + s0 + W[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t chv = Ch(e, f, g);
        uint32_t t1 = h + S1 + chv + K[i] + W[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t m = Maj(a, b, c);
        uint32_t t2 = S0 + m;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

} // namespace

void sha256(const unsigned char *input, size_t len,
            unsigned char out[SHA256_DIGEST_SIZE]) {
    uint32_t state[8];
    for (int i = 0; i < 8; ++i) state[i] = H0[i];

    // Process full 64-byte blocks.
    size_t pos = 0;
    while (pos + 64 <= len) {
        sha256Transform(input + pos, state);
        pos += 64;
    }

    // Pad the final block: append 0x80, zeros, then 64-bit big-endian bit
    // length.
    uint8_t block[64];
    std::memcpy(block, input + pos, len - pos);
    size_t rem = len - pos;
    block[rem] = 0x80;
    ++rem;
    if (rem > 56) {
        std::memset(block + rem, 0, 64 - rem);
        sha256Transform(block, state);
        rem = 0;
    }
    std::memset(block + rem, 0, 56 - rem);
    uint64_t bitLen = (uint64_t)len * 8;
    for (int i = 0; i < 8; ++i) {
        block[56 + i] = (uint8_t)((bitLen >> (56 - (i * 8))) & 0xFF);
    }
    sha256Transform(block, state);

    for (int i = 0; i < 8; ++i) {
        out[i * 4] = (state[i] >> 24) & 0xFF;
        out[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        out[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        out[i * 4 + 3] = (state[i] >> 0) & 0xFF;
    }
}

std::string sha256Hex(const std::string &input) {
    unsigned char digest[SHA256_DIGEST_SIZE];
    sha256(reinterpret_cast<const unsigned char *>(input.data()), input.size(),
           digest);

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < SHA256_DIGEST_SIZE; ++i) {
        out << std::setw(2) << static_cast<int>(digest[i]);
    }
    return out.str();
}

std::string deriveSha256(uint64_t version, const std::string &did,
                         const std::string &aspectName) {
    // Input: version (8-byte big-endian) || DID || aspectName
    std::string input;
    input.resize(8 + did.size() + aspectName.size());
    // Write version as 8-byte big-endian (most significant byte first)
    for (int i = 7; i >= 0; --i) {
        input[i] = static_cast<char>(version & 0xFF);
        version >>= 8;
    }
    std::memcpy(input.data() + 8, did.data(), did.size());
    std::memcpy(input.data() + 8 + did.size(), aspectName.data(),
                aspectName.size());
    return sha256Hex(input);
}

} // namespace keepsake::util