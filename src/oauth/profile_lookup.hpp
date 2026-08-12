#ifndef KEEPSAKE_OAUTH_PROFILE_LOOKUP_HPP
#define KEEPSAKE_OAUTH_PROFILE_LOOKUP_HPP

#include <string>
#include <vector>

namespace keepsake::oauth {

// Public, unauthenticated app.bsky.actor.getProfile lookup — no session
// needed, same as resolvePdsEndpoint's resolution calls. Returns false
// (leaving `createdAtIso` untouched) if the actor can't be resolved or the
// response is missing createdAt (the field only exists on
// profileViewDetailed, not the basic profile shapes wolfram's own typed
// wrappers expose — see AGENTS.md).
bool fetchAccountCreatedAt(const std::string &actorDidOrHandle,
                           std::string &createdAtIso);

struct FollowedActor {
    std::string did;
    std::string handle;
    std::string displayName; // may be empty
};

// Public, unauthenticated app.bsky.graph.getFollows lookup for the first
// page (up to `limit`) of accounts `actorDidOrHandle` follows. Returns
// false on any resolution/parse failure; a successful call with zero
// follows returns true with an empty list.
bool fetchFollows(const std::string &actorDidOrHandle, int limit,
                  std::vector<FollowedActor> &out);

} // namespace keepsake::oauth

#endif
