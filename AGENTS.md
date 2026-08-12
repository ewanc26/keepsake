# AGENTS.md

Guidance for AI coding agents working in this repository. Human contributors
may find it useful too, but the audience is agents.

## Project overview

A text-based RPG, currently **local-only**: a small room-graph, one NPC, one
boss, a short quest, a local JSON save. It is built as Phase 1 of a larger
design where the character sheet and quest progress eventually live as
records in the player's own AT Protocol PDS repo, synced via the
[wolfram](https://github.com/ewanc26/wolfram) SDK — but that identity layer
(Phase 2 onward) is not implemented yet. Do not assume any network code
exists; `sync/` currently has exactly one backend, `LocalRecordStore`, which
reads and writes a JSON file on disk.

- Language: C++23 throughout, no C in this repo (unlike wolfram, which is
  C23-core with a C++ RAII layer — Keepsake is a *consumer* of wolfram, not
  part of it).
- Build: CMake, no third-party dependencies. The JSON handling in `save/` is
  a small hand-rolled value type scoped to exactly what the save format
  needs — not a general-purpose parser, and not meant to become one.
- Target platforms: macOS and Linux desktop, matching the other native
  projects in this account (see `../rpg/AGENTS.md`). Windows is untested.

## Repository layout

```
src/
  identity/  identity.hpp/.cpp — key()/hash(), the one value everything
             persistent is keyed off (save path, worldSeed, and later the
             synced DID); see README.md "Identity"
  util/      xdg.hpp/.cpp — $XDG_DATA_HOME/keepsake resolution, shared by
             identity/ and save/
  world/     Location graph — rooms, exits, items, NPC/enemy placement
  entity/    Character and item definitions (structs, not systems)
  combat/    Turn-based combat resolution
  dialogue/  Branching NPC dialogue trees
  quest/     Quest-flag helpers over Progress
  save/      json.hpp/.cpp (minimal JSON value type), save.hpp/.cpp
             (Character/Progress <-> JSON, file I/O, path keyed by an
             identity hash)
  sync/      RecordStore interface (record_store.hpp) + LocalRecordStore
             (local_record_store.hpp/.cpp) — the seam a future
             WolframRecordStore plugs into
  ui/        Terminal command loop (terminal.hpp/.cpp)
  main.cpp
lexicons/click/croft/rpg/
  character.json, progress.json, event.json, achievement.json — the
  click.croft.rpg.* record schemas the identity layer will eventually write.
  Not consumed by anything yet; kept here so Phase 2 can feed them straight
  into wolfram's wf_lexgen_tool without re-deriving the shape.
```

## Module boundaries — read before editing

- **Everything that needs to name or seed something persistent goes through
  `identity::key()` / `identity::hash()`, not its own scheme.** If you add
  a new thing that needs a stable per-player identifier, key it off
  `identity::hash()` rather than inventing a second identity concept — the
  whole point of that module is that there is exactly one root value, so
  Phase 2 only has to change what `key()` returns, not every place that
  used to compute its own name.
- **`world/`, `entity/`, `combat/`, `dialogue/`, `quest/` must never include
  anything from `sync/`.** They operate on plain `Character`/`Progress`
  structs passed in by `ui/`. This is what keeps them unit-testable without
  a save file or a PDS in the loop, and it's what makes swapping
  `LocalRecordStore` for a wolfram-backed one later a change contained to
  `sync/` + `main.cpp`.
- `ui/terminal.cpp` is the only place that owns a `RecordStore&`. It calls
  `save()` on the `save`/`quit` commands and on `load()` at startup; nothing
  else touches persistence.
- `save/json.hpp` is intentionally minimal — it round-trips exactly the
  shapes `save/save.cpp` needs (object, array, string, number, bool). If a
  new field needs a JSON type this module doesn't support, extend it
  narrowly; don't reach for a third-party JSON library for a save format
  this small.

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/keepsake
```

There is no test suite yet. "Verified" means the build is clean and a
playthrough was actually run — walk from the gatehouse to the undercroft,
fight the boss, save, and reload — not that `cmake --build` exited 0.

## Code style

- Header guards (`KEEPSAKE_<MODULE>_<FILE>_HPP`), not `#pragma once` —
  matches the convention in `wolfram/cpp/wolfram-cpp/wolfram/*.hpp`.
- Format with `.clang-format` in this repo (copied from wolfram's: LLVM
  base, 4-space indent, 80 columns, attached braces).
- Comments explain *why*, sparingly. Do not narrate obvious code.
- Do not add exception-based control flow for expected game states (an
  empty inventory, an unknown command). Reserve exceptions/aborts for
  genuine programmer errors.

## Current reality and risks

- `combat::run` resolves an entire fight to completion in one call — there
  is no mid-fight prompt to drink a potion or flee. The boss's stats
  (`combat.cpp`'s `enemyRegistry()`) are tuned against a player who has
  already collected the armory's sword and is at full HP going in, *not*
  against arbitrary player state. If you touch player or enemy stats,
  re-run the playthrough in the build step above and confirm the intended
  path is still winnable without needing an interaction this module
  doesn't support.
- Combat, dialogue, and the quest flag on the boss are all content defined
  in `world/world.cpp`'s `World::createDefault()` — there is no external
  content format yet. Adding a room means editing that function directly.
- The save file has no versioning. If the `Character`/`Progress` shape
  changes, an old save will fail to parse; there is no migration path yet.
  Decide on a `saveVersion` field before this matters in practice (i.e.
  before Phase 2 changes the shape to match the lexicons).
- `lexicons/click/croft/rpg/*.json` describe the *intended* Phase 2 record
  shapes. Nothing in `src/` reads them yet — do not wire partial network
  code against them without also building the OAuth/session plumbing it
  depends on; a half-connected `sync/` backend that sometimes talks to a
  PDS and sometimes silently falls back to local is worse than not having
  one. See the design roadmap in `README.md`.

## Commits and pull requests

Matches the convention in `wolfram/AGENTS.md` — read that file if anything
here is ambiguous.

- **Atomic conventional commits**: every commit is exactly one logical
  change. Scope by module — `feat(world)`, `feat(combat)`, `fix(save)`,
  `fix(identity)`, `docs(readme)`, etc. Never combine a code change with a
  docs update, or changes to two unrelated modules, in one commit. Write
  the message to explain the reasoning, not just restate the file list.
  Split multi-concern work into sequential commits instead.
- **Feature branches, `--no-ff` merges**: land work on `feat/<area>` (or
  `fix/<area>`), merge to `main` with `git merge --no-ff` so the branch
  structure survives in history.
- **No AI co-authors**: commits must not carry a `Co-authored-by:` trailer
  crediting an AI agent, and must not reference the specific AI model used,
  in the commit message, a PR, or code comments. Credit for committed work
  goes to human authors only.
- **No commented-out code** left in place; delete dead code or move it to
  a test.
- Do not open a pull request unless explicitly asked.
