#ifndef KEEPSAKE_IDENTITY_IDENTITY_HPP
#define KEEPSAKE_IDENTITY_IDENTITY_HPP

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

// A short, stable, filesystem- and seed-safe digest of `key()` (16 lowercase
// hex characters, FNV-1a). Used anywhere the raw identity key would be
// awkward to embed directly — save file names, the deterministic world
// seed — without it needing to look like, or be confused with, a real DID
// string. Not a cryptographic hash; Phase 2's protocol-level work (record
// keys, session material) goes through wolfram's own primitives instead of
// this one.
std::string hash();

} // namespace keepsake::identity

#endif
