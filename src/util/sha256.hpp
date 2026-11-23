#ifndef KEEPSAKE_UTIL_SHA256_HPP
#define KEEPSAKE_UTIL_SHA256_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace keepsake::util {

// SHA-256 as specified in FIPS 180-4. Header-only usable: header declares
// the pure functions, implementation lives in src/util/sha256.cpp.

// Digest size in bytes.
constexpr size_t SHA256_DIGEST_SIZE = 32;

// Computes SHA-256 of `input` and writes 32 bytes into `out`.
void sha256(const unsigned char *input, size_t len,
            unsigned char out[SHA256_DIGEST_SIZE]);

// Convenience wrapper over a byte vector.
inline void sha256(const std::vector<unsigned char> &input,
                   unsigned char out[SHA256_DIGEST_SIZE]) {
    sha256(input.data(), input.size(), out);
}

// Convenience wrapper over a string. Returns the 32-byte digest as a
// lowercase hexadecimal string (64 characters).
std::string sha256Hex(const std::string &input);

// Convenience wrapper over several concatenated inputs. Returns the
// lowercase hex digest of version || did || aspect.
// sha256(version || DID || aspectName) as used by identity derivation.
std::string deriveSha256(uint64_t version, const std::string &did,
                         const std::string &aspectName);

} // namespace keepsake::util

#endif
