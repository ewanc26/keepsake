#include "world/world.hpp"

#include <algorithm>

namespace keepsake::world {

World World::createDefault() {
    World w;

    w.locations_["gatehouse"] = Location{
        "gatehouse",
        "the Gatehouse",
        "The gate hangs open on one hinge, rust weeping down the stone. "
        "Someone has been sitting watch here for a long time.",
        {{"north", "courtyard"}},
        {},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["courtyard"] = Location{
        "courtyard",
        "the Courtyard",
        "Weeds grow between old flagstones. An old man sits near a "
        "cracked well, watching the gate.",
        {{"south", "gatehouse"}, {"east", "armory"}, {"north", "chapel"}},
        {},
        std::string("watcher"),
        std::nullopt,
        {},
    };

    w.locations_["armory"] = Location{
        "armory",
        "the Armory",
        "Racks of rusted weapons line the walls, most too far gone to "
        "trust. One sword still holds an edge.",
        {{"west", "courtyard"}},
        {"iron_sword"},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["chapel"] = Location{
        "chapel",
        "the Chapel",
        "Dust motes drift through broken windows. A stair beyond the "
        "altar leads down into dark.",
        {{"south", "courtyard"}, {"down", "undercroft"}},
        {},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["undercroft"] = Location{
        "undercroft",
        "the Undercroft",
        "The air is cold and smells of old stone. Something is waiting "
        "here.",
        {{"up", "chapel"}},
        {},
        std::nullopt,
        std::string("hollow_knight"),
        {},
    };

    return w;
}

void World::reconcile(const quest::Progress &progress) {
    // `ui::terminal`'s `take` handler sets "item.taken.<locationId>.<itemId>"
    // for every pickup; drop anything so flagged so a taken item doesn't
    // reappear (and get taken, and applied, a second time) on reload.
    for (auto &entry : locations_) {
        Location &loc = entry.second;
        auto isTaken = [&](const std::string &itemId) {
            return quest::hasFlag(progress,
                                  "item.taken." + loc.id + "." + itemId);
        };
        loc.itemIds.erase(
            std::remove_if(loc.itemIds.begin(), loc.itemIds.end(), isTaken),
            loc.itemIds.end());
    }

    if (quest::hasFlag(progress, "quest.keep_cleared.complete")) {
        if (Location *undercroft = find("undercroft")) {
            undercroft->enemyId.reset();
        }
    }
}

const Location *World::find(const std::string &id) const {
    auto it = locations_.find(id);
    return it == locations_.end() ? nullptr : &it->second;
}

Location *World::find(const std::string &id) {
    auto it = locations_.find(id);
    return it == locations_.end() ? nullptr : &it->second;
}

} // namespace keepsake::world
