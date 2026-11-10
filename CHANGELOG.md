# Changelog

All notable changes to Keepsake are documented here. The version is
stamped into every build (see CMakeLists.txt's generated
`keepsake/version.hpp` and `keepsake version` for what a given binary is).

## Unreleased

Everything below is on `main` since the first playable state, ahead of the
first tagged release.

### Added

- **Seed-driven variance** — the worldSeed (your DID, or the local identity
  stand-in) now shapes world generation every session, signed in or not:
  deterministic hidden caches and a seeded description flourish
  (`World::applyWorldSeed`), plus a freshly-forced-gate flavor swap for
  brand-new accounts.
- **The Watchtower and "The Quiet Bell" side quest** — a new two-room area
  west of the courtyard, a carrion crow, the `brass spyglass` reward, and
  the Watcher pointing you up the stair if you ask him what else needs
  looking at. Fully optional; never gates the main line.
- **Readable lore notes** — journal pages and inscriptions the seed
  decides whether your world contains. `use` reads one aloud; they are
  re-readable and never consumed.
- **Live world events while playing** — signed-in play drains the
  firehose subscription between turns as "(Elsewhere)" ambient lines
  (`sync::EventBridge`), display-only by design.
- **Opt-in social-graph NPC mentions** — `keepsake npcs on` surfaces
  accounts you follow as flavor mentions in the Courtyard.
- **Build identifiers** — every build now stamps the project version and
  a 6-character short-SHA build id (see AGENTS.md's policy); surfaced via
  `keepsake version`.

### Tests

- New suites covering previously untested seams: `character_test`
  (entity/), `ui_test` (the terminal command loop, driven end to end with
  a recording `RecordStore` stub), `local_record_store_test` (the sync
  seam), and `version_test` (the build stamping); `firehose_decode_test`
  extended with a multi-block CAR fixture and `EventBridge`'s offline-safe
  lifecycle.
