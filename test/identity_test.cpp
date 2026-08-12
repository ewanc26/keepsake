// Unit tests for identity::key()/hash(). $XDG_DATA_HOME is redirected to
// throwaway temp directories for every case so this never touches (or
// depends on) the real ~/.local/share/keepsake identity file.

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

    fs::remove_all(base);

    std::cout << "identity_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
