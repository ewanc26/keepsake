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

    w.locations_["crypt_stair"] = Location{
        "crypt_stair",
        "the Crypt Stair",
        "A narrow stair spirals down past the Hollow Knight's post. Colder "
        "air rises from below, carrying the smell of wet stone and older "
        "things.",
        {{"up", "undercroft"}, {"down", "crypt"}},
        {},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["crypt"] = Location{
        "crypt",
        "the Old Crypt",
        "Bones lie scattered where they fell, undisturbed for longer than "
        "anyone living. A dagger, dropped by whoever was buried here, still "
        "lies among them.",
        {{"up", "crypt_stair"}, {"north", "bone_chamber"},
         {"east", "flooded_tunnel"}},
        {"rusty_dagger"},
        std::nullopt,
        std::string("skeletal_thrall"),
        {},
    };

    w.locations_["bone_chamber"] = Location{
        "bone_chamber",
        "the Bone Chamber",
        "Skulls are stacked floor to ceiling in careful rows, an old "
        "ossuary. A stiff leather cuirass hangs forgotten on a peg by the "
        "door.",
        {{"south", "crypt"}},
        {"leather_armor"},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["flooded_tunnel"] = Location{
        "flooded_tunnel",
        "the Flooded Tunnel",
        "Black water stands ankle-deep. Something large moves in it when "
        "you're not looking directly at it.",
        {{"west", "crypt"}, {"down", "sealed_vault"}},
        {},
        std::nullopt,
        std::string("tunnel_crawler"),
        {},
    };

    w.locations_["sealed_vault"] = Location{
        "sealed_vault",
        "the Sealed Vault",
        "The walls here are dry, deliberately built, deliberately hidden. "
        "Whatever the Rot Warden has been guarding, it's been guarding it "
        "for a very long time.",
        {{"up", "flooded_tunnel"}},
        {},
        std::nullopt,
        std::string("rot_warden"),
        {},
    };

    return w;
}

void World::reconcile(quest::Progress &progress) {
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
            // With the Hollow Knight fallen, the stair it was guarding
            // opens — add the exit once rather than duplicate it on every
            // reconcile() call.
            bool hasDown = std::any_of(
                undercroft->exits.begin(), undercroft->exits.end(),
                [](const Exit &e) { return e.direction == "down"; });
            if (!hasDown) {
                undercroft->exits.push_back(Exit{"down", "crypt_stair"});
            }
        }
        quest::setFlag(progress, "quest.the_keepsake.started");
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
