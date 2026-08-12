#include "identity/identity.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "save/json.hpp"
#include "util/xdg.hpp"

namespace keepsake::identity {

namespace {

std::string identityFilePath() {
    return util::keepsakeDataDir() + "/identity";
}

// Reads the DID straight out of a persisted OAuth session file, if one
// exists — tokenSet.sub in the shape wf_oauth_session_state_serialize
// writes. Deliberately does this with keepsake's own JSON parser rather
// than depending on wolfram: identity/ is core (always compiled, even
// with KEEPSAKE_WITH_WOLFRAM=OFF), and this is a pure local file read, no
// network involved, so it doesn't need wolfram's OAuth machinery at all —
// see util::oauthSessionFilePath()'s doc comment.
std::string signedInDid() {
    std::ifstream in{util::oauthSessionFilePath()};
    if (!in) return "";
    std::ostringstream buf;
    buf << in.rdbuf();

    save::Json parsed;
    if (!save::Json::parse(buf.str(), parsed) || !parsed.isObject()) return "";
    const save::Json *tokenSet = parsed.find("tokenSet");
    if (tokenSet == nullptr || !tokenSet->isObject()) return "";
    const save::Json *sub = tokenSet->find("sub");
    if (sub == nullptr) return "";
    return sub->asString();
}

std::string generateLocalAnchor() {
    std::mt19937_64 engine{std::random_device{}()};
    std::uniform_int_distribution<int> nibble(0, 15);
    static const char *hex = "0123456789abcdef";

    std::string out = "local:";
    for (int i = 0; i < 16; ++i) {
        out.push_back(hex[nibble(engine)]);
    }
    return out;
}

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// FNV-1a, 64-bit. Not cryptographic — see identity.hpp.
uint64_t fnv1a(const std::string &s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return h;
}

} // namespace

std::string key() {
    if (std::string did = signedInDid(); !did.empty()) return did;

    const std::string path = identityFilePath();

    if (std::ifstream in{path}; in) {
        std::ostringstream buf;
        buf << in.rdbuf();
        std::string existing = trim(buf.str());
        if (!existing.empty()) return existing;
    }

    std::string anchor = generateLocalAnchor();
    if (std::ofstream out{path, std::ios::trunc}; out) {
        out << anchor << "\n";
    }
    return anchor;
}

std::string hash() {
    uint64_t h = fnv1a(key());
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(h));
    return std::string(buf);
}

} // namespace keepsake::identity
