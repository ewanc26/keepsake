# Contributing to keepsake

A text-based RPG in C++ where your AT Protocol DID is the save file. The local game (the keep, its crypt and Abyss, the spider den, the quest lines, bosses, the talk-down-not-fight resolution of the Nameless Thing) is fully playable offline; signed in, the same character sheet and quest progress live as `click.croft.rpg.*` records in the player's own PDS. Thanks for wanting to help!

Independent project; see the [trademark notice](TRADEMARKS.md).

## Getting started

Build per the [README](README.md) (C++ with CMake). CI runs on every push.

## Notes for this project

- **Offline-first is a design pillar.** The game is complete without any network; every record-sync feature has to be optional on top of that, never a dependency of play.
- Dialogue trees and quests must resolve consistently when they switch between talking and fighting; the Nameless Thing's two resolutions lead to the same flag and reward — preserve that equivalence if you touch quest logic.
- Save-data changes need explicit handling: existing players' save records in their PDS repos must not be silently broken.
- Test sync paths against a scratch PDS account; never log credentials.
- The tone is TA, atmosphere, folklore; writing contributions should match the existing voice rather than genericise it.
- Conventional commits; focused PRs; say how you verified.

## Licence

See [LICENSE](LICENSE). Contributions accepted under the same terms.