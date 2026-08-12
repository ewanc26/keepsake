#include "oauth/profile_lookup.hpp"

#include <wolfram/xrpc.h>

#include "oauth/url_encode.hpp"
#include "save/json.hpp"

namespace keepsake::oauth {

namespace {

// The well-known public, read-only AppView — no auth, no PDS-hosting
// account needed. Same unauthenticated-read pattern as
// resolvePdsEndpoint's resolution calls in oauth_flow.cpp.
constexpr const char *kPublicAppView = "https://public.api.bsky.app";

bool publicQuery(const std::string &nsid, const std::string &queryString,
                 save::Json &out) {
    wf_xrpc_client *client = wf_xrpc_client_new(kPublicAppView);
    if (client == nullptr) return false;

    std::string url =
        std::string(kPublicAppView) + "/xrpc/" + nsid + "?" + queryString;
    wf_response resp{};
    bool ok = wf_http_get(client, url.c_str(), &resp) == WF_OK &&
              resp.status == 200 && resp.body != nullptr;
    if (ok) ok = save::Json::parse(resp.body, out) && out.isObject();

    wf_response_free(&resp);
    wf_xrpc_client_free(client);
    return ok;
}

} // namespace

bool fetchAccountCreatedAt(const std::string &actorDidOrHandle,
                           std::string &createdAtIso) {
    save::Json body;
    if (!publicQuery("app.bsky.actor.getProfile",
                     "actor=" + urlEncode(actorDidOrHandle), body)) {
        return false;
    }
    const save::Json *createdAt = body.find("createdAt");
    if (createdAt == nullptr) return false;
    createdAtIso = createdAt->asString();
    return !createdAtIso.empty();
}

bool fetchFollows(const std::string &actorDidOrHandle, int limit,
                  std::vector<FollowedActor> &out) {
    save::Json body;
    std::string query = "actor=" + urlEncode(actorDidOrHandle) +
                        "&limit=" + std::to_string(limit);
    if (!publicQuery("app.bsky.graph.getFollows", query, body)) return false;

    const save::Json *follows = body.find("follows");
    if (follows == nullptr || !follows->isArray()) return false;

    out.clear();
    for (const auto &item : follows->items()) {
        if (!item.isObject()) continue;
        const save::Json *did = item.find("did");
        const save::Json *handle = item.find("handle");
        if (did == nullptr || handle == nullptr) continue;

        FollowedActor actor;
        actor.did = did->asString();
        actor.handle = handle->asString();
        if (const save::Json *displayName = item.find("displayName")) {
            actor.displayName = displayName->asString();
        }
        out.push_back(std::move(actor));
    }
    return true;
}

} // namespace keepsake::oauth
