# Changelog

All notable changes to Keepsake are documented here. The version is
stamped into every build (see CMakeLists.txt's generated
`keepsake/version.hpp` and `keepsake version` for what a given binary is).

## 0.4.0 - 2026-11-23

First tagged release. Everything below is on `main` since the first playable
state.

### Added

- **Portable character bundles** — `keepsake export <path>` writes a
  versioned JSON envelope around the schema-versioned character and progress
  records, and `keepsake import <path>` loads it into the current account
  (refusing to replace an existing character without `--force`). Newer
  bundles and newer saves are reported and never downgraded. `--offline` on
  either command, plus `keepsake offline` to play, forces the local store so
  the whole flow works with no network.
- **Save schema versioning** — `click.croft.rpg.character`/`.progress`
  records and the local save now carry an explicit `schemaVersion`. Saves
  from before the field load through an ordered migration table (absent
  means the initial shape), a save written by a newer schema is reported
  and left untouched instead of being downgraded, and host tests run every
  migration over a fixture of the initial shape.
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
- **The thing below has been learning your mark** — the open letter's
  warning, paid off: scratched into the crypt stair is a short, shaking
  copy of your own mark; the abyssal stair bears a longer, steadier
  practice wall; and scored across the floor of the abyss is your full
  mark with exactly one character wrong. The echoes are derived from the
  mark itself (your identity hash — the same line cut over the gate),
  so the same identity always finds the same practice on the same walls,
  signed in or not.
- **The mark, finished** — once the Nameless Thing is done with (either
  branch of the last quest), the learning stops and the practice is
  completed properly: cut clean beside the attempts that taught it, your
  full mark, every character correct, and nothing added since. The
  wrong-carved record is never erased — only finished.
- **Teaching the locket** — the open letter's other instruction, now a
  verb: `use silver locket` speaks your own mark into the hollow (the
  identity hash this save already wanders under), once and one-way — the
  second use reads back what it knows, and the learned name survives in
  the save's flags. A character with no mark yet finds the locket
  waiting, not full of air.
- **The abyss reads the locket** — with the locket taught, the Nameless
  Thing's conversation opens one option more: hold it up, and the thing
  reads the hollow the way anyone can — a name that has been *given*, to
  a keeper, on purpose — and names, at last, the one thing down here it
  cannot wear. The doorway out is the peaceful one it already offered.
  Dialogue options can now be gated on a quest flag (`requireFlag`),
  which keeps them and their numbering out of every conversation that
  has not earned them.
- **A premise, dramatised** — every world now opens with the open letter
  waiting under a stone in the gatehouse: it names the mystery (the
  "keepsake" is a locket of the old make that remembers names), the
  stakes (something below lost its own name and has decided to wear
  someone else's), and the identity hook — the player's own save-file
  identifier, cut into the lintel over the gate in every build, signed
  in or not. The opening, the Watcher, and the quest beats were rewritten
  around it.
- **Live world events while playing** — signed-in play drains the
  firehose subscription between turns as "(Elsewhere)" ambient lines
  (`sync::EventBridge`), display-only by design.
- **Opt-in social-graph NPC mentions** — `keepsake npcs on` surfaces
  accounts you follow as flavor mentions in the Courtyard.
- **Build identifiers** — every build now stamps the project version and
  a 6-character short-SHA build id (see AGENTS.md's policy); surfaced via
  `keepsake version`.

### Changed

- **Versioned identity derivation** — the character's `worldSeed` is now the
  documented SHA-256 KDF (`be64(version) || DID || aspect`) instead of the
  ad-hoc FNV-1a identity hash, and every character records the
  `derivationVersion` beside its seed. Existing characters keep their stored
  seed and are stamped version 0 (legacy) by the v2 save migration; new ones
  use version 1 and the `world` aspect. `keepsake seeds [did]` prints the
  exact values for a bug report. A future derivation bump is an explicit
  world fork, never a silent one.
- **Wolfram v0.45.0** — the FetchContent pin moved from v0.44.2 to
  v0.45.0. The public surface Keepsake uses is unchanged, including the
  CAR/firehose decode path.

### Tests

- New suites covering previously untested seams: `character_test`
  (entity/), `ui_test` (the terminal command loop, driven end to end with
  a recording `RecordStore` stub), `local_record_store_test` (the sync
  seam), and `version_test` (the build stamping); `firehose_decode_test`
  extended with a multi-block CAR fixture and `EventBridge`'s offline-safe
  lifecycle.
