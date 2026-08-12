#ifndef KEEPSAKE_OAUTH_LOOPBACK_LISTENER_HPP
#define KEEPSAKE_OAUTH_LOOPBACK_LISTENER_HPP

#include <string>

namespace keepsake::oauth {

struct CallbackResult {
    bool ok = false;
    // The raw (still percent-encoded) query string after '?' in the
    // request line, e.g. "state=...&code=...&iss=...". Empty on failure.
    std::string rawQuery;
    // Set when ok is false: "timeout", "bind failed", etc.
    std::string error;
};

// Binds a POSIX TCP listener on 127.0.0.1:`port`, blocks until exactly one
// HTTP GET request arrives — the OAuth redirect — responds with a minimal
// static page, and returns the request line's query string. Gives up after
// `timeoutSeconds` with ok=false. macOS/Linux only, matching this project's
// target platforms (see README).
CallbackResult waitForCallback(int port, int timeoutSeconds);

} // namespace keepsake::oauth

#endif
