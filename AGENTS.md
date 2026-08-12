# AGENTS.md

Guidance for AI coding agents working in this repository. Human contributors
may find it useful too, but the audience is agents.

## Project overview

A text-based RPG: a small room-graph, one NPC, one boss, a short quest,
playable fully offline — and, once signed in, backed by
[wolfram](https://github.com/ewanc26/wolfram) so the same character sheet
and quest progress live as `click.croft.rpg.*` records in the player's own
AT Protocol PDS repo. Both backends implement the same `sync::RecordStore`
interface; the game plays identically either way.

- Language: C++23 throughout, no C in this repo (unlike wolfram, which is
  C23-core with a C++ RAII layer — Keepsake is a *consumer* of wolfram, not
  part of it). `oauth/`, `sync/wolfram_record_store.*`, and
  `sync/firehose_watch.*` are compiled only when `KEEPSAKE_WITH_WOLFRAM=ON`
  (the default); everything else has no third-party dependency at all.
- Build: CMake. `save/`'s JSON handling is a small hand-rolled value type
  scoped to exactly what the save format needs — not a general-purpose
  parser, and not meant to become one; it's also reused by the wolfram-backed
  code (record bodies, discovery documents) rather than pulling in a second
  JSON library for that.
- wolfram itself is fetched automatically if there's no checkout at
  `../wolfram` — see CMakeLists.txt. No manual setup step required.
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
  world/     Location graph — rooms, exits, items, NPC/enemy placement;
             Location::flavorNpcNames for opt-in social-graph mentions
             (display-only, not interactive — populated fresh in main.cpp
             each run, never persisted)
  entity/    Character and item definitions (structs, not systems)
  combat/    Turn-based combat resolution
  dialogue/  Branching NPC dialogue trees
  quest/     Quest-flag helpers over Progress
  save/      json.hpp/.cpp (minimal JSON value type), save.hpp/.cpp
             (Character/Progress <-> JSON, file I/O, path keyed by an
             identity hash)
  sync/      RecordStore interface (record_store.hpp); LocalRecordStore
             (local file); WolframRecordStore (click.croft.rpg.character/
             .progress via generic repo CRUD, plus recordAchievement()/
             recordEvent() broadcasts); firehose_watch.* (standalone
             `keepsake events` firehose reader — not wired into the live
             game loop, see "Current reality" below)
  oauth/     url_encode.*, loopback_listener.* (single-request local HTTP
             server for the OAuth redirect), oauth_flow.* (AuthSession,
             signIn(), restoreSession() — see its header for the full
             flow and the wolfram bug it works around), profile_lookup.*
             (public, unauthenticated app.bsky.actor.getProfile /
             .graph.getFollows — no session needed, used by both
             identity-seeded flavor and social-graph NPCs)
  ui/        Terminal command loop (terminal.hpp/.cpp)
  main.cpp   Subcommand dispatch (login/logout/whoami/events) + backend
             selection for the default play mode
lexicons/click/croft/rpg/
  character.json, progress.json, event.json, achievement.json — the
  click.croft.rpg.* record schemas WolframRecordStore reads/writes.
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
./build/keepsake              # play — synced if signed in, local otherwise
./build/keepsake login <handle-or-did>
./build/keepsake whoami
./build/keepsake logout
./build/keepsake events       # watch the firehose for click.croft.rpg.event
```

Add `-DKEEPSAKE_WITH_WOLFRAM=OFF` to build the local-only game with no
network code at all.

`ctest` runs unit tests for `quest`, `combat`, `save/json`, `save`,
`dialogue`, and `identity` (`quest_test`/`combat_test`/`json_test`/
`save_test`/`dialogue_test`/`identity_test` — registered unconditionally,
no wolfram link needed), plus `firehose_decode_test` under
`KEEPSAKE_WITH_WOLFRAM=ON`. `world/` and `entity/`'s item/combat helpers
are exercised indirectly through `combat_test`/`dialogue_test`/`save_test`
but have no dedicated test file yet; there is also still no automated test
for the game loop itself (`ui/terminal.cpp`). "Verified" for end-to-end
play means the build is clean, `ctest` passes, and a playthrough was
actually run — walk from the gatehouse to the undercroft, fight the boss,
save, and reload — not that `cmake --build` exited 0. For anything
touching `oauth/`/`sync/wolfram_record_store.*`, "verified" means run
against real infrastructure where possible (discovery, resolution, and
the firehose connect/stop lifecycle all can be, without a login) — see
"Current reality and risks" for exactly what has and hasn't been exercised
that way.

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
- The save file (and the `click.croft.rpg.character`/`.progress` records)
  have no versioning. If the `Character`/`Progress` shape changes, an old
  save will fail to parse; there is no migration path yet.
- **`oauth/oauth_flow.cpp`'s `discoverMetadata()` works around a real bug
  in wolfram, not a design choice.** `wf_oauth_discover` (and the
  `wf_oauth_resource_metadata_get`/`wf_oauth_server_metadata_get` it calls)
  goes through `wf_oauth_json_array`, which rejects a *present but empty*
  JSON array even on an optional field — confirmed in wolfram's own source
  (`src/session/oauth/util.c`), and confirmed live: a real Bluesky-hosted
  PDS returns `"scopes_supported":[]`, which made every discovery attempt
  fail with `WF_ERR_PARSE`. `discoverMetadata()` fetches and parses both
  discovery documents itself instead and populates the wolfram structs by
  hand. If a future wolfram release fixes this, `discoverMetadata()` can be
  deleted in favor of calling `wf_oauth_discover` directly — check first,
  don't assume it's still needed.
- **`AuthSession` is neither copyable nor movable, on purpose.**
  `wf_auth_client` retains pointers into its owned fields for its whole
  lifetime. Every caller holds it as a stable member (see
  `WolframRecordStore`) or a stack local that's never relocated — do not
  add a move constructor to "fix" a compile error without checking whether
  the fix is actually to stop trying to move it.
- **The `keepsake login` flow is verified only up to the point requiring
  the player's own browser approval** — resolution, discovery, PAR, and
  authorization-URL construction were confirmed against live Bluesky
  infrastructure during development (a real PAR request, a real
  `bsky.social/oauth/authorize` URL). The token exchange
  (`wf_oauth_authorization_complete`) and everything after it (session
  persistence, `WolframRecordStore` reads/writes) have not been exercised
  against a completed real login, because that requires a human clicking
  "Authorize" — an agent cannot do this on someone's behalf. Treat that
  path as implemented-and-reasoned-through, not proven, until someone
  actually runs `keepsake login` and plays a synced session.
- **`sync/firehose_watch.cpp` (the `keepsake events` command) is verified
  only partially.** The connect/retry/Ctrl+C-stop lifecycle is confirmed
  against the real firehose. The CAR/CBOR record-decode path
  (`wf_car_parse` → `wf_car_find_block` → `wf_cbor_parse` → walking the
  `wf_cbor_item` map) could **not** be verified against live data in the
  development sandbox — `wss://bsky.network` WebSocket connections failed
  there (`on_error` reported "websocket connect failed") even though plain
  HTTPS to the same infrastructure worked fine for the OAuth flow, which
  points at a sandbox network-egress restriction on WebSocket upgrades
  specifically, not a code defect. It also has nothing to decode yet in
  practice: no other `click.croft.rpg.event` writers exist. Before trusting
  this path, test it somewhere WebSocket egress is unrestricted, ideally
  against a real event written by a signed-in session.
- **The firehose reader is deliberately not wired into the live game
  loop.** `keepsake events` is a separate, standalone command
  (`main.cpp`), not a background thread inside `ui::run`'s interactive
  loop. Folding remote events into `World`/`Progress` while the player is
  mid-game needs real synchronization design (a mutex or message channel
  between the firehose callback's thread and the main loop) that doesn't
  exist yet — do not bolt a background subscription onto `ui::run` without
  designing that first; a data race on `World` is worse than the feature
  not existing.

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
