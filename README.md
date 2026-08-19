# Keepsake

A text-based RPG in C++ where your AT Protocol DID is the save file.

> Independent project; see the [trademark notice](TRADEMARKS.md).

## Status

The local game (fourteen rooms across the keep, the crypt beneath it, a
side-passage spider den, and a final descent into the Abyss; an NPC; four
enemies plus three bosses; three linked quests) is fully playable offline.
The last boss, the Nameless Thing, can be talked down instead of fought —
its dialogue tree resolves that quest and hands over its reward peacefully,
or (declining, or attacking outright) the same quest resolves through
combat instead, with the same reward as loot either way; which path you
took is just a different route to the same flag. Signed in, the same
character sheet and quest progress live as `click.croft.rpg.*` records in
your own PDS, and each quest's completion — however it resolved — broadcasts
a verifiable achievement plus a world event other players' clients could
someday react to. See [Roadmap](#roadmap) for exactly what's built, what's
verified against live infrastructure, and what's designed but not yet wired
up.

## Concept

You explore a small, ruined keep — and, once you clear it, the crypt buried
beneath it, and beneath that, whatever the crypt's own guardian was itself
guarding — as a `wf`-flavoured text adventure: rooms, an NPC, three bosses,
a three-part quest chasing down the Watcher's stolen keepsake to its source.
The ending isn't fixed: the last boss can be reasoned with as easily as
fought, and the game reacts to whichever the player chooses rather than
funneling both toward one outcome. Signed in, your character sheet and
quest progress are ordinary records in your own PDS repo — the game follows
your DID to any machine you sign into, and no Keepsake server ever needs to
exist. Defeating (or, for the last one, talking down) a boss writes a
`click.croft.rpg.achievement` record (verifiable by anyone, directly from
your repo) and a `click.croft.rpg.event` record broadcasting it over the
firehose — the beginning of a shared world where other players' actions
could someday
ripple into yours.

## Identity

Every persistent thing in Keepsake — the save file's location, the
character's `worldSeed`, and (once signed in) the AT Protocol repo a save
syncs to — is keyed off one value: `identity::key()`. Nothing downstream
chooses its own naming scheme independently of it. Signed out, that value is
a locally generated stand-in (`local:<16 hex chars>`, persisted at
`.../keepsake/identity`) — clearly not a DID, and never sent anywhere.
Signed in, `key()` reads the DID straight out of the persisted OAuth session
(no network call needed) — every module that already keyed off it needed no
further change. See `src/identity/identity.hpp`.

## Requirements

- A C++23 compiler (Apple Clang, GCC ≥ 13, or Clang ≥ 16)
- CMake ≥ 3.20
- Nothing else to install by hand: with `KEEPSAKE_WITH_WOLFRAM=ON` (the
  default), configure uses a local checkout at `../wolfram` if one exists,
  or fetches wolfram's latest tagged GitHub release automatically otherwise.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Pass `-DKEEPSAKE_WITH_WOLFRAM=OFF` for a dependency-free, local-only build.

## Play

```bash
./build/keepsake                        # play — synced if signed in, local otherwise
./build/keepsake login <handle-or-did>   # sign in with your AT Protocol handle
./build/keepsake whoami                  # show the signed-in DID, if any
./build/keepsake logout                  # forget the saved session
./build/keepsake events                  # watch the firehose for other players' world events
./build/keepsake npcs on                 # show accounts you follow as in-world mentions
./build/keepsake npcs off
```

`login` prints an authorization URL, opens your browser at it, and waits for
the redirect — approve it there, the same as signing into any AT Protocol
app.

In-game commands:

| Command | Effect |
|---|---|
| `look` | Describe the current location |
| `go <direction>` | Move through an exit (e.g. `go north`) |
| `take <item>` | Pick up an item from the current location |
| `use <item>` | Consume or apply an item from your inventory |
| `talk <npc>` | Start a dialogue with an NPC present |
| `attack` | Fight the enemy present at the current location |
| `inventory` / `i` | List carried items |
| `stats` | Show level, HP, XP, attack, defense |
| `quests` | List known quest flags and their state |
| `save` | Write the current character and progress to disk (or your PDS, if signed in) |
| `help` | List commands |
| `quit` | Save and exit |

Signed out, progress saves to
`$XDG_DATA_HOME/keepsake/saves/<identity-hash>.json` (falling back to
`$HOME/.local/share/keepsake/...`). Signed in, it saves to
`click.croft.rpg.character`/`.progress` in your own PDS instead — see
[Identity](#identity).

## Project layout

```
src/
  world/     Location graph — rooms, exits, items, NPC/enemy placement
  entity/    Character and item definitions
  combat/    Turn-based combat resolution
  dialogue/  Branching NPC dialogue trees
  quest/     Quest-flag state helpers
  save/      Minimal JSON value type + local save (de)serialization
  identity/  identity::key()/hash() — see "Identity" above
  oauth/     OAuth login flow: handle/DID/PDS resolution, the loopback
             redirect, PKCE/DPoP via wolfram, session persistence;
             profile_lookup.* for the public, unauthenticated reads behind
             identity-seeded flavor and social-graph NPCs
  sync/      RecordStore interface; LocalRecordStore (local file) and
             WolframRecordStore (your PDS) both implement it; firehose_watch
             backs the `events` command and EventBridge, the background
             subscription signed-in play polls for "(Elsewhere) ..." text
  ui/        Terminal command loop
  main.cpp   Subcommands + backend selection
lexicons/
  click/croft/rpg/   The click.croft.rpg.* record schemas WolframRecordStore
                     reads and writes
```

## Roadmap

1. **Local core** — done. The whole game, playable with a local save, zero
   network dependency.
2. **Identity** — done, verified up to the point requiring your own
   browser approval. OAuth login via [wolfram](https://github.com/ewanc26/wolfram)
   (resolution, discovery, and the authorization request confirmed against
   live Bluesky infrastructure during development), `click.croft.rpg.character`/
   `.progress` records, cross-device resume. The token exchange and
   PDS reads/writes are implemented and reasoned through, but exercising
   them end to end needs an actual `keepsake login` run by a human — an
   agent can't click "Authorize" on someone's behalf.
3. **Dynamic tuning** — done, modestly. The DID-seeded `worldSeed`,
   verifiable `click.croft.rpg.achievement` records, and a brand-new
   account seeing a freshly-forced gate instead of the default worn one
   (`fetchAccountCreatedAt`, verified live against a real account) are all
   in. Deeper world-*generation* variance from account signal, beyond this
   one flavor swap, is still just the design's idea.
4. **Shared world** — mostly done. Quest completion broadcasts a
   `click.croft.rpg.event` record. `keepsake events` watches the firehose
   for them (connect/retry/stop lifecycle confirmed against the real
   firehose) — but the record-decode path hasn't been verified against live
   data (see `AGENTS.md`). Opt-in social-graph NPCs from your follows are
   in — `keepsake npcs on`, verified live with a real account's real
   follows appearing in the Courtyard — but purely as flavor mentions, not
   interactive NPCs. *Remote* events are now folded into your own running
   game, signed in: a background subscription (`sync::EventBridge`) feeds
   `ui::run` lines to print between turns, deliberately display-only —
   nothing off the firehose ever touches `World`/`Progress`, so this
   doesn't need (and doesn't have) full thread-safe game-state sharing.

See `AGENTS.md`'s "Current reality and risks" for the unabridged, honest
version of exactly what's tested versus reasoned-through versus aspirational.

## Support

If you find this project useful, consider supporting its development:

[![Ko-fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/ewancroft)
[![GitHub Sponsors](https://img.shields.io/badge/GitHub%20Sponsors-30363D?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sponsors/ewanc26)

## Licence

AGPL-3.0
