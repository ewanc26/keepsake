# Keepsake

A text-based RPG in C++ where your AT Protocol DID is the save file.

> Independent project; see the [trademark notice](TRADEMARKS.md).

## Status

**Phase 1 of the roadmap below: a fully playable local text RPG, no network
dependency.** Signing in with an AT Protocol handle, DID-rooted saves, and
the shared-world firehose layer are designed but not yet implemented — see
[Roadmap](#roadmap).

## Concept

You explore a small, ruined keep as a `wf`-flavoured text adventure: rooms,
an NPC, a boss, a short quest. Once the identity layer lands (Phase 2), the
same character sheet and quest progress will be written as ordinary records
in your own PDS repo — the game follows your DID to any machine you sign
into, and no Keepsake server ever needs to exist. Phase 4 goes further:
world-altering events other players write ripple into your game over the
AT Protocol firehose, so the world keeps moving even when you're not
playing.

## Identity

Every persistent thing in Keepsake — the save file's location, the
character's `worldSeed`, and eventually the AT Protocol repo a save syncs
to — is keyed off one value: `identity::key()`. Nothing downstream chooses
its own naming scheme independently of it. Right now, with no sign-in
implemented, that value is a locally generated stand-in (`local:<16 hex
chars>`, persisted at `.../keepsake/identity`) — clearly not a DID, and
never sent anywhere. Phase 2 changes what `key()` returns to the real
signed-in DID; every module that already keys off it — the save path, the
world seed — needs no further change, because they were never looking at
anything but that one function. See `src/identity/identity.hpp`.

## Requirements

- A C++23 compiler (Apple Clang, GCC ≥ 13, or Clang ≥ 16)
- CMake ≥ 3.20

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Play

```bash
./build/keepsake
```

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
| `save` | Write the current character and progress to disk |
| `help` | List commands |
| `quit` | Save and exit |

Progress saves to `$XDG_DATA_HOME/keepsake/saves/<identity-hash>.json`
(falling back to `$HOME/.local/share/keepsake/...` if `XDG_DATA_HOME` isn't
set). The identity hash is derived from a locally generated, persisted
stand-in identity at `.../keepsake/identity` — the same value that becomes
your DID once sign-in exists (Phase 2). See [Identity](#identity).

## Project layout

```
src/
  world/     Location graph — rooms, exits, items, NPC/enemy placement
  entity/    Character and item definitions
  combat/    Turn-based combat resolution
  dialogue/  Branching NPC dialogue trees
  quest/     Quest-flag state helpers
  save/      Minimal JSON value type + local save (de)serialization
  sync/      RecordStore interface; LocalRecordStore is the only
             implementation so far — a WolframRecordStore backed by the
             wolfram AT Protocol SDK is Phase 2
  ui/        Terminal command loop
lexicons/
  click/croft/rpg/   The click.croft.rpg.* record schemas Phase 2 will
                     write to your PDS, ready to feed wolfram's
                     wf_lexgen_tool once that phase starts
```

## Roadmap

1. **Local core** (done) — the whole game, playable with a local save,
   zero network dependency.
2. **Identity** — OAuth login via [wolfram](https://github.com/ewanc26/wolfram),
   `click.croft.rpg.character` / `.progress` records, cross-device resume.
3. **Dynamic tuning** — identity-seeded world variance, verifiable
   `click.croft.rpg.achievement` records.
4. **Shared world** — subscribe to `click.croft.rpg.event` writes over the
   firehose so other players' actions shift your world; opt-in social-graph
   NPCs from your follows.
5. **Stretch** — cross-compiled offline-mode builds for the exotic targets
   wolfram already supports (Wii, Wii U, 3DS).

Phases 2–5 are designed (see `lexicons/` for the record schemas and each
module's `sync/` seam) but not yet built.

## Support

If you find this project useful, consider supporting its development:

[![Ko-fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/ewancroft)
[![GitHub Sponsors](https://img.shields.io/badge/GitHub%20Sponsors-30363D?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sponsors/ewanc26)

## Licence

AGPL-3.0
