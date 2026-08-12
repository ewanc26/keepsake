#include "oauth/oauth_flow.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

#include <wolfram/identity.h>

#include "oauth/loopback_listener.hpp"
#include "oauth/url_encode.hpp"
#include "save/json.hpp"
#include "util/xdg.hpp"

namespace keepsake::oauth {

namespace {

// This app's identity: every collection it might ever write to, requested
// up front. Narrower than a blanket `repo:*` grant, matching the pattern
// atproto-snake uses for its one collection.
const std::string kScope = "atproto "
                           "repo:click.croft.rpg.character "
                           "repo:click.croft.rpg.progress "
                           "repo:click.croft.rpg.event "
                           "repo:click.croft.rpg.achievement";

// Fixed so restoreSession() always reconstructs the identical client_id a
// prior signIn() used — see oauth_flow.hpp.
constexpr int kCallbackPort = 51823;
constexpr int kCallbackTimeoutSeconds = 300;

std::string redirectUri() {
    return "http://127.0.0.1:" + std::to_string(kCallbackPort) + "/callback";
}

std::string loopbackClientId() {
    return "http://localhost?redirect_uri=" + urlEncode(redirectUri()) +
           "&scope=" + urlEncode(kScope);
}

// RAII around a value-typed wolfram struct freed by `Free(&value)`. Move-out
// via release() zeroes the local copy so its own destructor becomes a
// harmless no-op — the same "freed/zeroed struct frees safely" contract
// every wolfram _free function documents.
template <typename T, void (*Free)(T *)> class StructGuard {
  public:
    StructGuard() : value_{} {}
    ~StructGuard() {
        Free(&value_);
    }
    StructGuard(const StructGuard &) = delete;
    StructGuard &operator=(const StructGuard &) = delete;

    T *get() {
        return &value_;
    }
    T &ref() {
        return value_;
    }
    T release() {
        T moved = value_;
        value_ = T{};
        return moved;
    }

  private:
    T value_;
};

using ResourceMetadataGuard =
    StructGuard<wf_oauth_resource_metadata, wf_oauth_resource_metadata_free>;
using ServerMetadataGuard =
    StructGuard<wf_oauth_server_metadata, wf_oauth_server_metadata_free>;
using ClientMetadataGuard =
    StructGuard<wf_oauth_client_metadata, wf_oauth_client_metadata_free>;
using BeginResultGuard = StructGuard<wf_oauth_authorization_begin_result,
                                     wf_oauth_authorization_begin_result_free>;
using CompleteResultGuard =
    StructGuard<wf_oauth_authorization_complete_result,
                wf_oauth_authorization_complete_result_free>;

std::string readFile(const std::string &path) {
    std::ifstream in(path);
    if (!in) return "";
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

bool writeFile(const std::string &path, const std::string &content) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << content;
    return static_cast<bool>(out);
}

// Best-effort; a failure here just means the player has to copy the URL
// themselves, which is why the flow always prints it too.
void openBrowser(const std::string &url) {
#if defined(__APPLE__)
    const char *opener = "open";
#else
    const char *opener = "xdg-open";
#endif
    pid_t pid = fork();
    if (pid == 0) {
        execlp(opener, opener, url.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

// Resolves `handleOrDid` to a DID and that DID's *current* PDS service
// endpoint. Uses a throwaway bootstrap transport — wolfram's resolution
// calls ignore the transport's own base URL, deriving the PLC directory /
// did:web host from the DID itself (or DNS/well-known for a handle).
bool resolvePdsEndpoint(const std::string &handleOrDid, std::string &didOut,
                        std::string &pdsEndpointOut, std::string &error) {
    wf_xrpc_client *bootstrap = wf_xrpc_client_new("https://bsky.social");
    if (!bootstrap) {
        error = "failed to create resolver transport";
        return false;
    }

    std::string did;
    if (handleOrDid.rfind("did:", 0) == 0) {
        did = handleOrDid;
    } else {
        char *resolved = nullptr;
        if (wf_handle_resolve(bootstrap, handleOrDid.c_str(), &resolved) !=
                WF_OK ||
            !resolved) {
            error = "couldn't resolve handle " + handleOrDid;
            wf_xrpc_client_free(bootstrap);
            return false;
        }
        did = resolved;
        std::free(resolved);
    }

    wf_did_document doc{};
    wf_status status = wf_did_resolve(bootstrap, did.c_str(), &doc);
    wf_xrpc_client_free(bootstrap);
    if (status != WF_OK || !doc.pds_endpoint) {
        error = "couldn't resolve a PDS endpoint for " + did;
        wf_did_document_free(&doc);
        return false;
    }

    didOut = did;
    pdsEndpointOut = doc.pds_endpoint;
    wf_did_document_free(&doc);
    return true;
}

// Builds an owned wf_oauth_string_list from a JSON array of strings (empty
// array in, empty-but-valid list out — unlike wolfram's own parser, see
// discoverMetadata below).
wf_oauth_string_list stringListFrom(const save::Json *arr) {
    wf_oauth_string_list list{};
    if (arr == nullptr || !arr->isArray()) return list;
    const auto &items = arr->items();
    if (items.empty()) return list;
    list.count = items.size();
    list.items = static_cast<char **>(malloc(items.size() * sizeof(char *)));
    for (size_t i = 0; i < items.size(); ++i) {
        list.items[i] = strdup(items[i].asString().c_str());
    }
    return list;
}

// Works around a real bug in wolfram's discovery parsing: internally,
// wf_oauth_discover -> wf_oauth_resource_metadata_get /
// wf_oauth_server_metadata_get both go through wf_oauth_json_array, which
// rejects a *present but empty* JSON array even for an optional field
// (`if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) == 0) return
// WF_ERR_PARSE;`, unconditional on `required`). Real PDS hosts return
// exactly that shape in practice — e.g. `"scopes_supported":[]` — verified
// live against a Bluesky-hosted PDS during development, where this made
// every discovery attempt fail with WF_ERR_PARSE despite a well-formed,
// spec-compliant response. Rather than patch wolfram (out of scope here,
// and other in-progress work is checked out against it), fetch and parse
// both discovery documents directly with keepsake's own JSON parser, and
// populate the wolfram structs by hand — matching their owned-string
// contract exactly (every field a plain strdup'd char*), so
// wf_oauth_resource_metadata_free/wf_oauth_server_metadata_free and every
// downstream wolfram call that only *reads* these structs (begin,
// complete, session_refresh) work completely unmodified.
bool discoverMetadata(wf_xrpc_client *transport, const std::string &resourceUrl,
                      wf_oauth_resource_metadata &resourceOut,
                      wf_oauth_server_metadata &serverOut, std::string &error) {
    resourceOut = wf_oauth_resource_metadata{};
    serverOut = wf_oauth_server_metadata{};

    std::string resourceReqUrl =
        resourceUrl + "/.well-known/oauth-protected-resource";
    wf_response resourceResp{};
    bool ok = wf_http_get(transport, resourceReqUrl.c_str(), &resourceResp) ==
                  WF_OK &&
              resourceResp.status == 200 && resourceResp.body != nullptr;
    save::Json resourceJson;
    std::string resourceField, issuerField;
    if (ok && (ok = save::Json::parse(resourceResp.body, resourceJson) &&
                    resourceJson.isObject())) {
        const save::Json *resourceValue = resourceJson.find("resource");
        const save::Json *serversValue =
            resourceJson.find("authorization_servers");
        ok = resourceValue != nullptr && serversValue != nullptr &&
             serversValue->isArray() && !serversValue->items().empty();
        if (ok) {
            resourceField = resourceValue->asString();
            issuerField = serversValue->items()[0].asString();
        }
    }
    wf_response_free(&resourceResp);
    if (!ok) {
        error = "failed to fetch/parse " + resourceReqUrl;
        return false;
    }
    resourceOut.resource = strdup(resourceField.c_str());
    resourceOut.authorization_servers.count = 1;
    resourceOut.authorization_servers.items =
        static_cast<char **>(malloc(sizeof(char *)));
    resourceOut.authorization_servers.items[0] = strdup(issuerField.c_str());

    std::string serverReqUrl =
        issuerField + "/.well-known/oauth-authorization-server";
    wf_response serverResp{};
    ok = wf_http_get(transport, serverReqUrl.c_str(), &serverResp) == WF_OK &&
         serverResp.status == 200 && serverResp.body != nullptr;
    save::Json serverJson;
    std::string issuerStr, authEndpoint, tokenEndpoint, parEndpoint;
    if (ok && (ok = save::Json::parse(serverResp.body, serverJson) &&
                    serverJson.isObject())) {
        const save::Json *issuerV = serverJson.find("issuer");
        const save::Json *authV = serverJson.find("authorization_endpoint");
        const save::Json *tokenV = serverJson.find("token_endpoint");
        const save::Json *parV =
            serverJson.find("pushed_authorization_request_endpoint");
        ok = issuerV && authV && tokenV && parV;
        if (ok) {
            issuerStr = issuerV->asString();
            authEndpoint = authV->asString();
            tokenEndpoint = tokenV->asString();
            parEndpoint = parV->asString();
        }
    }
    if (!ok) {
        error = "failed to fetch/parse " + serverReqUrl;
        wf_response_free(&serverResp);
        wf_oauth_resource_metadata_free(&resourceOut);
        return false;
    }
    serverOut.issuer = strdup(issuerStr.c_str());
    serverOut.authorization_endpoint = strdup(authEndpoint.c_str());
    serverOut.token_endpoint = strdup(tokenEndpoint.c_str());
    serverOut.pushed_authorization_request_endpoint =
        strdup(parEndpoint.c_str());
    if (const save::Json *revokeV = serverJson.find("revocation_endpoint")) {
        serverOut.revocation_endpoint = strdup(revokeV->asString().c_str());
    }
    if (const save::Json *issSupported =
            serverJson.find("authorization_response_iss_parameter_supported")) {
        serverOut.authorization_response_iss_parameter_supported =
            issSupported->asBool(false) ? 1 : 0;
    }
    // Read by wf_oauth_client_auth_validate (must contain "none" for our
    // public, no-signing-key client) — everything else discoverMetadata
    // populates is read directly by begin/complete/refresh only.
    serverOut.token_endpoint_auth_methods_supported = stringListFrom(
        serverJson.find("token_endpoint_auth_methods_supported"));
    wf_response_free(&serverResp);
    return true;
}

std::string queryParam(const std::string &rawQuery, const std::string &key) {
    size_t pos = 0;
    while (pos < rawQuery.size()) {
        size_t amp = rawQuery.find('&', pos);
        size_t end = amp == std::string::npos ? rawQuery.size() : amp;
        std::string pair = rawQuery.substr(pos, end - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key) {
            return urlDecode(pair.substr(eq + 1));
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return "";
}

} // namespace

void AuthSession::release() {
    if (authClient_) {
        wf_auth_client_free(authClient_);
        authClient_ = nullptr;
    }
    wf_oauth_session_state_free(&session_);
    session_ = wf_oauth_session_state{};
    wf_oauth_server_metadata_free(&server_);
    server_ = wf_oauth_server_metadata{};
    if (transport_) {
        wf_xrpc_client_free(transport_);
        transport_ = nullptr;
    }
    clientId_.clear();
    did_.clear();
}

AuthSession::~AuthSession() {
    release();
}

std::string sessionFilePath() {
    return util::oauthSessionFilePath();
}

bool AuthSession::build(wf_xrpc_client *transport,
                        wf_oauth_server_metadata server, std::string clientId,
                        wf_oauth_session_state session, AuthSession &out,
                        std::string &error) {
    out.transport_ = transport;
    out.server_ = server;
    out.clientId_ = std::move(clientId);
    out.session_ = session;
    out.did_ = out.session_.subject ? out.session_.subject : "";

    wf_oauth_client_auth clientAuth{};
    clientAuth.client_id = out.clientId_.c_str();
    // NULL: only meaningful for private_key_jwt clients, which we
    // never are (public loopback client, signing_key always NULL).
    clientAuth.authorization_server_issuer = nullptr;
    clientAuth.signing_key = nullptr;
    clientAuth.key_id = nullptr;

    out.authClient_ = wf_auth_client_new(out.transport_, &out.session_,
                                         &out.server_, &clientAuth);
    if (!out.authClient_) {
        error = "failed to construct authenticated client";
        out.release();
        return false;
    }
    return true;
}

bool restoreSession(const std::string &sessionPath, AuthSession &out,
                    std::string &error) {
    std::string sessionJson = readFile(sessionPath);
    if (sessionJson.empty()) {
        error = "no session file yet";
        return false;
    }

    wf_oauth_session_state session{};
    if (wf_oauth_session_state_parse(sessionJson.c_str(), sessionJson.size(),
                                     nullptr, &session) != WF_OK) {
        error = "session file is corrupt";
        return false;
    }

    std::string resolvedDid, pdsEndpoint;
    if (!resolvePdsEndpoint(session.subject ? session.subject : "", resolvedDid,
                            pdsEndpoint, error)) {
        wf_oauth_session_state_free(&session);
        return false;
    }

    wf_xrpc_client *transport = wf_xrpc_client_new(pdsEndpoint.c_str());
    if (!transport) {
        error = "failed to create transport";
        wf_oauth_session_state_free(&session);
        return false;
    }

    ResourceMetadataGuard resource;
    ServerMetadataGuard server;
    if (!discoverMetadata(transport, session.issuer, *resource.get(),
                          *server.get(), error)) {
        wf_oauth_session_state_free(&session);
        wf_xrpc_client_free(transport);
        return false;
    }

    std::string clientId = loopbackClientId();
    int64_t now = static_cast<int64_t>(time(nullptr));
    if (session.expires_at > 0 && now >= session.expires_at) {
        wf_oauth_client_auth clientAuth{};
        clientAuth.client_id = clientId.c_str();
        clientAuth.authorization_server_issuer = nullptr;
        clientAuth.signing_key = nullptr;
        clientAuth.key_id = nullptr;

        if (wf_oauth_session_refresh(transport, server.get(), &clientAuth,
                                     &session, now) != WF_OK) {
            error = "session refresh failed — try `keepsake login` again";
            wf_oauth_session_state_free(&session);
            wf_xrpc_client_free(transport);
            return false;
        }
        char *refreshedJson = nullptr;
        if (wf_oauth_session_state_serialize(&session, &refreshedJson) ==
                WF_OK &&
            refreshedJson) {
            writeFile(sessionPath, refreshedJson);
            std::free(refreshedJson);
        }
    }

    return AuthSession::build(transport, server.release(), clientId, session,
                              out, error);
}

bool signIn(const std::string &handleOrDid, const std::string &sessionPath,
            AuthSession &out, std::string &error) {
    std::string resolvedDid, pdsEndpoint;
    if (!resolvePdsEndpoint(handleOrDid, resolvedDid, pdsEndpoint, error)) {
        return false;
    }
    std::cout << "Resolved " << handleOrDid << " to " << resolvedDid << ", PDS "
              << pdsEndpoint << "\n"
              << std::flush;

    wf_xrpc_client *transport = wf_xrpc_client_new(pdsEndpoint.c_str());
    if (!transport) {
        error = "failed to create transport";
        return false;
    }

    ResourceMetadataGuard resource;
    ServerMetadataGuard server;
    if (!discoverMetadata(transport, pdsEndpoint, *resource.get(),
                          *server.get(), error)) {
        wf_xrpc_client_free(transport);
        return false;
    }

    std::string redirect = redirectUri();
    std::string clientId = loopbackClientId();

    ClientMetadataGuard client;
    if (wf_oauth_client_metadata_get(transport, clientId.c_str(),
                                     client.get()) != WF_OK) {
        error = "failed to build loopback client metadata";
        wf_xrpc_client_free(transport);
        return false;
    }

    wf_oauth_client_auth clientAuth{};
    clientAuth.client_id = clientId.c_str();
    clientAuth.authorization_server_issuer = nullptr;
    clientAuth.signing_key = nullptr;
    clientAuth.key_id = nullptr;

    wf_oauth_authorization_begin_options opts{};
    opts.redirect_uri = redirect.c_str();
    opts.scope = kScope.c_str();
    opts.login_hint = handleOrDid.empty() ? nullptr : handleOrDid.c_str();
    opts.now = static_cast<int64_t>(time(nullptr));
    opts.state_ttl = 600;

    BeginResultGuard begin;
    if (wf_status beginStatus =
            wf_oauth_authorization_begin(transport, server.get(), client.get(),
                                         &clientAuth, &opts, begin.get());
        beginStatus != WF_OK) {
        error = "failed to start the authorization request (wf_status=" +
                std::to_string(beginStatus) + ")";
        wf_xrpc_client_free(transport);
        return false;
    }

    std::cout << "\nOpen this URL to sign in (opening your browser "
                 "now):\n"
              << begin.ref().authorization_url << "\n\n"
              << std::flush;
    openBrowser(begin.ref().authorization_url);

    std::cout << "Waiting for the browser to redirect back on port "
              << kCallbackPort << " (" << kCallbackTimeoutSeconds
              << "s timeout)...\n"
              << std::flush;
    CallbackResult callback =
        waitForCallback(kCallbackPort, kCallbackTimeoutSeconds);
    if (!callback.ok) {
        error = callback.error;
        wf_xrpc_client_free(transport);
        return false;
    }

    std::string state = queryParam(callback.rawQuery, "state");
    std::string code = queryParam(callback.rawQuery, "code");
    std::string issuer = queryParam(callback.rawQuery, "iss");
    std::string err = queryParam(callback.rawQuery, "error");
    std::string errDesc = queryParam(callback.rawQuery, "error_description");

    wf_oauth_callback_params cbParams{};
    cbParams.response = nullptr;
    cbParams.state = state.empty() ? nullptr : state.c_str();
    cbParams.code = code.empty() ? nullptr : code.c_str();
    cbParams.issuer = issuer.empty() ? nullptr : issuer.c_str();
    cbParams.error = err.empty() ? nullptr : err.c_str();
    cbParams.error_description = errDesc.empty() ? nullptr : errDesc.c_str();

    CompleteResultGuard complete;
    wf_status status = wf_oauth_authorization_complete(
        transport, server.get(), client.get(), &clientAuth, &cbParams,
        begin.ref().state, begin.ref().state_json,
        std::strlen(begin.ref().state_json), redirect.c_str(),
        static_cast<int64_t>(time(nullptr)), complete.get());

    if (status != WF_OK || complete.ref().error != nullptr) {
        error = "authorization failed";
        if (complete.ref().error) {
            error += ": " + std::string(complete.ref().error);
            if (complete.ref().error_description) {
                error += " — " + std::string(complete.ref().error_description);
            }
        }
        wf_xrpc_client_free(transport);
        return false;
    }

    if (complete.ref().session_json) {
        writeFile(sessionPath, complete.ref().session_json);
    }

    wf_oauth_session_state session = complete.ref().session;
    complete.ref().session = wf_oauth_session_state{};

    return AuthSession::build(transport, server.release(), clientId, session,
                              out, error);
}

} // namespace keepsake::oauth
