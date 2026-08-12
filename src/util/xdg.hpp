#ifndef KEEPSAKE_UTIL_XDG_HPP
#define KEEPSAKE_UTIL_XDG_HPP

#include <string>

namespace keepsake::util {

// Returns $XDG_DATA_HOME/keepsake, falling back to $HOME/.local/share/
// keepsake, creating it (and any missing parents) if it doesn't exist yet.
std::string keepsakeDataDir();

// <data dir>/oauth-session.json. A well-known path rather than something
// only the (optional) oauth/ module knows about, so identity::key() can
// check for a signed-in DID without depending on wolfram — see
// identity.cpp.
std::string oauthSessionFilePath();

} // namespace keepsake::util

#endif
