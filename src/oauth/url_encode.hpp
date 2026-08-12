#ifndef KEEPSAKE_OAUTH_URL_ENCODE_HPP
#define KEEPSAKE_OAUTH_URL_ENCODE_HPP

#include <string>

namespace keepsake::oauth {

// Percent-encodes everything except RFC 3986 unreserved characters
// (A-Za-z0-9-._~). Used to embed redirect_uri/scope as query parameters on
// the "http://localhost?..." loopback client_id — see oauth_flow.hpp.
std::string urlEncode(const std::string &value);

// Decodes %XX escapes and '+' as space. Used to read the OAuth callback's
// query parameters (state/code/iss), which the auth server percent-encodes.
std::string urlDecode(const std::string &value);

} // namespace keepsake::oauth

#endif
