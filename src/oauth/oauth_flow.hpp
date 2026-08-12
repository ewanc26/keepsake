#ifndef KEEPSAKE_OAUTH_OAUTH_FLOW_HPP
#define KEEPSAKE_OAUTH_OAUTH_FLOW_HPP

#include <string>

#include <wolfram/auth_client.h>
#include <wolfram/oauth.h>
#include <wolfram/xrpc.h>

namespace keepsake::oauth {

// A live, authenticated session: an XRPC transport, the discovered auth
// server's metadata, and the OAuth session bound to it, wrapped in a
// wf_auth_client that handles DPoP proofs, nonce rotation, and refresh.
//
// Deliberately neither copyable nor movable — wf_auth_client retains
// pointers into this object's owned fields for its whole lifetime (see
// auth_client.h), so an AuthSession must live at a fixed address from
// construction to destruction. Callers hold it as a stable member (see
// sync::WolframRecordStore), never by value.
class AuthSession {
  public:
    AuthSession() = default;
    ~AuthSession();
    AuthSession(const AuthSession &) = delete;
    AuthSession &operator=(const AuthSession &) = delete;
    AuthSession(AuthSession &&) = delete;
    AuthSession &operator=(AuthSession &&) = delete;

    bool valid() const {
        return authClient_ != nullptr;
    }
    wf_auth_client *client() const {
        return authClient_;
    }
    const std::string &did() const {
        return did_;
    }

  private:
    friend bool restoreSession(const std::string &, AuthSession &,
                               std::string &);
    friend bool signIn(const std::string &, const std::string &, AuthSession &,
                       std::string &);

    void release();

    // Common tail of restoreSession()/signIn(): takes ownership of
    // `transport`/`server`/`session` and builds the wf_auth_client. A
    // private member (not a free function) purely so it can reach these
    // fields directly instead of needing its own friend declaration.
    static bool build(wf_xrpc_client *transport,
                      wf_oauth_server_metadata server, std::string clientId,
                      wf_oauth_session_state session, AuthSession &out,
                      std::string &error);

    wf_xrpc_client *transport_ = nullptr;
    wf_oauth_server_metadata server_{};
    std::string clientId_;
    wf_oauth_session_state session_{};
    wf_auth_client *authClient_ = nullptr;
    std::string did_;
};

// <data dir>/oauth-session.json — where a completed sign-in is persisted.
std::string sessionFilePath();

// Loads a previously persisted session, resolves the signed-in DID's
// *current* PDS endpoint fresh (not cached — the player may have moved
// providers since last login), refreshes the access token first if
// expired, and populates `out` (which must already be at its final
// address — see AuthSession). false + `error` set on any failure,
// including "no session file yet", the expected steady state before the
// player ever runs `keepsake login`.
bool restoreSession(const std::string &sessionPath, AuthSession &out,
                    std::string &error);

// Runs the full interactive flow for `handleOrDid`: resolves it to a DID
// and PDS endpoint, discovers that PDS's auth server, builds a loopback
// client_id embedding this app's scope (every click.croft.rpg.*
// collection, see oauth_flow.cpp) and a redirect_uri on a fixed local
// port, prints the authorization URL and opens the system browser at it,
// blocks on a local HTTP listener for the redirect, exchanges the code,
// persists the session to `sessionPath`, and populates `out`. This step
// requires the player's own live approval in their browser; nothing here
// can complete without it. The scope and port are fixed constants (not
// parameters) so restoreSession() can always reconstruct the identical
// client_id a prior signIn() used.
bool signIn(const std::string &handleOrDid, const std::string &sessionPath,
            AuthSession &out, std::string &error);

} // namespace keepsake::oauth

#endif
