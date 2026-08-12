#ifndef KEEPSAKE_UTIL_XDG_HPP
#define KEEPSAKE_UTIL_XDG_HPP

#include <string>

namespace keepsake::util {

// Returns $XDG_DATA_HOME/keepsake, falling back to $HOME/.local/share/
// keepsake, creating it (and any missing parents) if it doesn't exist yet.
std::string keepsakeDataDir();

} // namespace keepsake::util

#endif
