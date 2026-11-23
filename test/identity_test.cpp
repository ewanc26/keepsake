// Unit tests for identity::key()/hash(). $XDG_DATA_HOME is redirected to
// throwaway temp directories for every case so this never touches (or
// depends on) the real ~/.local/share/keepsake identity file.

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "identity/identity.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at identity_test.cpp:" << __LINE__ << "\n";         \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using namespace keepsake::identity;
    namespace fs = std::filesystem;

    fs::path base = fs::temp_directory_path() / "keepsake_identity_test";
    fs::remove_all(base);

    // A freshly generated local anchor has the documented shape and
    // persists across calls (second call reads the file back rather than
    // regenerating).
    {
        fs::path dir = base / "fresh";
        setenv("XDG_DATA_HOME", dir.string().c_str(), 1);

        std::string first = key();
        CHECK(first.rfind("local:", 0) == 0);
        CHECK(first.size() == 6 + 16); // "local:" + 16 hex chars

        std::string second = key();
        CHECK(first == second);
    }

    // hash() is deterministic for a fixed key() and is a 16-character
    // lowercase hex digest (FNV-1a, 64-bit).
    {
        fs::path dir = base / "hash_stable";
        setenv("XDG_DATA_HOME", dir.string().c_str(), 1);

        std::string h1 = hash();
        std::string h2 = hash();
        CHECK(h1 == h2);
        CHECK(h1.size() == 16);
        for (char c : h1) {
            CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        }
    }

    // Two independent identity directories get different local anchors
    // (astronomically likely to differ — 16 random hex chars).
    {
        fs::path dirA = base / "anchor_a";
        setenv("XDG_DATA_HOME", dirA.string().c_str(), 1);
        std::string keyA = key();

        fs::path dirB = base / "anchor_b";
        setenv("XDG_DATA_HOME", dirB.string().c_str(), 1);
        std::string keyB = key();

        CHECK(keyA != keyB);
    }

    // A signed-in session's DID takes priority over the local anchor, and
    // key() needs no network call to read it — a plain
    // oauth-session.json with tokenSet.sub is enough (see
    // identity.cpp's signedInDid()).
    {
        fs::path dir = base / "signed_in";
        setenv("XDG_DATA_HOME", dir.string().c_str(), 1);
        fs::create_directories(dir / "keepsake");

        std::ofstream session(dir / "keepsake" / "oauth-session.json");
        session << R"({"tokenSet":{"sub":"did:plc:testaccount1234"}})";
        session.close();

        CHECK(key() == "did:plc:testaccount1234");
    }

    // The documented derivation (#10): fixed vectors computed independently
    // with Python hashlib over be64(version) || DID || aspect. A refactor
    // that changes the bytes cannot pass silently; the same DID yields these
    // exact values on every machine.
    {
        struct Vector {
            uint64_t version;
            const char *did;
            const char *aspect;
            const char *hex;
        };
        const Vector vectors[] = {
            {1, "did:plc:testaccount1234", "world",
             "6aca338882d30b069c6ff2264f21c592de4e6904a9038565286d783ecdd803a8"},
            {1, "did:plc:testaccount1234", "mark",
             "75e835a6f23b7ceed2395f36ee524ba50ad4861aabde3fda0ddb915d44aa6206"},
            {1, "did:web:example.com", "world",
             "f5d6fe9da4ebab9ee4a334af52a57eec20126d2b91b2ce4350292746011fafb3"},
            {1, "local:0123456789abcdef", "flourish",
             "1151c8ce3ed9c8c41aa5753fe6dde76a316d36297bcd24d11cab1f0a152c6c01"},
            {2, "did:plc:testaccount1234", "world",
             "095207f214f008eff3f8089023aa57b8c3682ade031ab952dc888acdb05a266f"},
        };
        for (const Vector &vector : vectors) {
            CHECK(deriveSeed(vector.version, vector.did, vector.aspect) ==
                  vector.hex);
        }

        // Version gating and aspect separation: one input changing must
        // change the output, and the version must fork the world visibly.
        CHECK(deriveSeed(1, "did:plc:testaccount1234", "world") !=
              deriveSeed(2, "did:plc:testaccount1234", "world"));
        CHECK(deriveSeed(1, "did:plc:testaccount1234", "world") !=
              deriveSeed(1, "did:plc:testaccount1234", "mark"));
        CHECK(deriveSeed(1, "did:plc:testaccount1234", "world") !=
              deriveSeed(1, "did:web:example.com", "world"));

        // Every seed is a 64-character lowercase hex SHA-256 digest.
        const std::string seed =
            deriveSeed(kDerivationVersion, "did:plc:testaccount1234",
                       kWorldSeedAspect);
        CHECK(seed.size() == 64);
        for (char c : seed) {
            CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        }
    }

    // deriveSeed(aspect) uses the current key() and kDerivationVersion, so
    // a signed-in DID gets exactly the documented fixture.
    {
        fs::path dir = base / "derive_current";
        setenv("XDG_DATA_HOME", dir.string().c_str(), 1);
        fs::create_directories(dir / "keepsake");

        std::ofstream session(dir / "keepsake" / "oauth-session.json");
        session << R"({"tokenSet":{"sub":"did:plc:testaccount1234"}})";
        session.close();

        CHECK(key() == "did:plc:testaccount1234");
        CHECK(deriveSeed(kWorldSeedAspect) ==
              "6aca338882d30b069c6ff2264f21c592de4e6904a9038565286d783ecdd803a8");
        CHECK(deriveSeed(kDerivationVersion, key(), kWorldSeedAspect) ==
              deriveSeed(kWorldSeedAspect));
        CHECK(deriveSeed(kWorldSeedAspect) != deriveSeed(kMarkAspect));
    }

    fs::remove_all(base);

    std::cout << "identity_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
