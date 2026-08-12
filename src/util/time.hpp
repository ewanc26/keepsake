#ifndef KEEPSAKE_UTIL_TIME_HPP
#define KEEPSAKE_UTIL_TIME_HPP

#include <string>

namespace keepsake::util {

// Current UTC time as an AT Protocol-compatible ISO 8601 datetime string
// (e.g. "2026-08-12T18:00:00Z").
std::string isoNow();

} // namespace keepsake::util

#endif
