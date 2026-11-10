#ifndef KEEPSAKE_WORLD_WORLD_HPP
#define KEEPSAKE_WORLD_WORLD_HPP

#include <unordered_map>

#include "quest/quest.hpp"
#include "world/location.hpp"

namespace keepsake::world {

class World {
  public:
    // Builds the one hand-authored keep this phase ships with. There is no
    // external content format yet — see AGENTS.md.
    static World createDefault();

    // World content (which enemies/items are still present) isn't part of
    // the save — only quest flags and location are. Call this once after
    // loading a save, before the player can act, so a defeated boss or a
    // taken item doesn't reappear just because it wasn't re-derived from
    // `progress`. Content-specific by nature (it knows the undercroft's
    // enemy is tied to the keep_cleared quest); extend it alongside
    // whatever new content ties world state to a flag.
    // Takes `progress` by non-const reference: some reconciliations (e.g.
    // the crypt stair opening up) also set the flag that starts the next
    // quest.
    void reconcile(quest::Progress &progress);

    // Deterministic worldSeed-driven variance (design roadmap §3's deeper
    // world-generation variance — the gatehouse flavor swap in main.cpp was
    // the only seed-derived content before this). Given the same worldSeed
    // (see entity::Character, set at creation from identity::hash()) this
    // always produces the same world: a couple of hidden caches tucked into
    // seeded locations, plus a seeded description flourish. Call it once
    // after createDefault(), before reconcile() — the placements are
    // re-derived identically every run, so the generic `item.taken.<loc>.<item>`
    // flags reconcile() already drops items on keep seeded caches from
    // respawning after they're taken. Idempotent: calling it twice applies
    // the same variance once.
    void applyWorldSeed(const std::string &worldSeed);

    // The premise's payoff (see the open letter): the thing below has
    // been learning your mark, and its practice is cut into the walls —
    // a shaky partial copy on the crypt stair, a longer steadier one on
    // the abyssal stair, and in the abyss itself your full mark with
    // exactly one character wrong. Deterministic from the mark (your
    // identity's hash, the same one on the lintel) rather than the
    // worldSeed, because what it learned is you, not this world's dice.
    // Call once after createDefault(), alongside applyWorldSeed();
    // idempotent, in the same way — the echoes find their own text
    // already appended and add nothing.
    void applyMarkEchoes(const std::string &mark);

    // The mark-echoes' endpoint: once the Nameless Thing is done (either
    // branch of quest.the_nameless sets the same flag), the practice
    // stops and the mark is finished properly — the wrong character cut
    // again, correct, beside the attempts that taught it. Same
    // determinism as applyMarkEchoes: the resolution is derived from the
    // mark, and its own opening words make it idempotent. Call it after
    // reconcile(), only when quest.the_nameless.complete is set.
    void resolveMarkEchoes(const std::string &mark);

    const Location *find(const std::string &id) const;
    Location *find(const std::string &id);

  private:
    std::unordered_map<std::string, Location> locations_;
};

} // namespace keepsake::world

#endif
