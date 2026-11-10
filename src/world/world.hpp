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

    const Location *find(const std::string &id) const;
    Location *find(const std::string &id);

  private:
    std::unordered_map<std::string, Location> locations_;
};

} // namespace keepsake::world

#endif
