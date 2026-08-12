#ifndef KEEPSAKE_WORLD_LOCATION_HPP
#define KEEPSAKE_WORLD_LOCATION_HPP

#include <optional>
#include <string>
#include <vector>

namespace keepsake::world {

struct Exit {
    std::string direction;
    std::string targetId;
};

struct Location {
    std::string id;
    std::string name;
    std::string description;
    std::vector<Exit> exits;
    // Items currently present; `take` removes an entry.
    std::vector<std::string> itemIds;
    std::optional<std::string> npcId;
    // Cleared to nullopt once the enemy present here is defeated.
    std::optional<std::string> enemyId;
    // Display-only mentions of accounts the signed-in player follows —
    // opt-in (`keepsake npcs on`), populated once in main.cpp from a
    // public app.bsky.graph.getFollows lookup. Not interactive: no
    // dialogue tree, no `talk` target — see AGENTS.md for why this stays
    // flavor-only rather than full NPCs for now.
    std::vector<std::string> flavorNpcNames;
};

} // namespace keepsake::world

#endif
