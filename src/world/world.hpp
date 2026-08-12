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
    void reconcile(const quest::Progress &progress);

    const Location *find(const std::string &id) const;
    Location *find(const std::string &id);

  private:
    std::unordered_map<std::string, Location> locations_;
};

} // namespace keepsake::world

#endif
