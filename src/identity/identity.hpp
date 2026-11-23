#ifndef KEEPSAKE_IDENTITY_IDENTITY_HPP
#define KEEPSAKE_IDENTITY_IDENTITY_HPP

#include <cstdint>
#include <string>

namespace keepsake::identity {

// The identity this run is keyed under. Everything persistent — the save
// file's location, the character's worldSeed, and (from Phase 2 onward)
// the AT Protocol repo a save syncs to — derives from this one value, not
// from independent choices made in each module. That's deliberate: once
// sign-in exists, `key()` becomes the signed-in player's DID and nothing
// downstream has to change, because it was never looking at anything but
// this function.
//
// Phase 1 has no sign-in, so this returns a locally generated, persisted
// stand-in of the form "local:<16 hex chars>" — never a real DID, and
// never sent anywhere. It lives at `<data dir>/identity` and is created on
// first run. Phase 2's `sync::WolframRecordStore` is expected to prefer a
// real signed-in DID when a session exists and fall back to this only when
// running signed out.
std::string key();

// A short, stable, filesystem-safe digest of `key()` (16 lowercase hex
// characters, FNV-1a). It anchors local save file names and the mark cut
// over the gate, so existing saves keep resolving to the same file. It is
// deliberately not the world derivation any more (see deriveSeed below and
// Character::derivationVersion): it is not a cryptographic hash, and the
// versioned KDF is the documented rule.
std::string hash();

// ---------------------------------------------------------------------------
// Identity derivation (#10)
//
// The worldSeed is the documented, versioned derivation
//
//     seed(version, key, aspect) =
//         lowercase hex SHA-256( be64(version) || UTF-8(key) || UTF-8(aspect) )
//
// where be64 is exactly 8 bytes, most significant byte first. The version
// integer is first in the input so a future rule can only ever produce a
// different value, never silently reinterpret an old one. Changing the
// algorithm, the byte order or the layout requires bumping
// kDerivationVersion and is a deliberate world fork: the same DID then grows
// a different world. Characters record the version that produced their seed
// (Character::derivationVersion), so an old world remains attributable after
// the derivation moves on.
// ---------------------------------------------------------------------------

// Version 1: SHA-256(be64(1) || key || aspect), lowercase hex. Version 0 is
// reserved for the legacy FNV-1a identity hash that predates this rule; it is
// never passed to deriveSeed and only appears as a stored character value.
inline constexpr uint64_t kDerivationVersion = 1;

// The documented aspects. `world` is the character's worldSeed; `mark` and
// `flourish` name the premise's identity-seeded surfaces for the debug
// command and for future derivation consumers.
inline constexpr const char *kWorldSeedAspect = "world";
inline constexpr const char *kMarkAspect = "mark";
inline constexpr const char *kFlourishAspect = "flourish";

// Pure form: derives from an explicit identity key and version. Used by the
// fixtures and by `keepsake seeds <did>` so both name exactly the same rule.
std::string deriveSeed(uint64_t version, const std::string &identityKey,
                       const std::string &aspect);

// Derives from the current key() and kDerivationVersion.
std::string deriveSeed(const std::string &aspect);

} // namespace keepsake::identity

#endif
