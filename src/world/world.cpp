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
        "door. A gap behind the ossuary racks leads further in.",
        {{"south", "crypt"}, {"east", "collapsed_passage"}},
        {"leather_armor"},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["collapsed_passage"] = Location{
        "collapsed_passage",
        "the Collapsed Passage",
        "Rubble chokes half the tunnel. Thick strands of webbing span the "
        "gap that's left, catching the little light that reaches this "
        "far.",
        {{"west", "bone_chamber"}, {"north", "spider_den"}},
        {},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["spider_den"] = Location{
        "spider_den",
        "the Spider Den",
        "The walls are lost under layers of silk. Old bones, picked "
        "clean, are wrapped and hung like ornaments.",
        {{"south", "collapsed_passage"}},
        {},
        std::nullopt,
        std::string("cave_spider"),
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

    w.locations_["abyssal_stair"] = Location{
        "abyssal_stair",
        "the Abyssal Stair",
        "Steps cut too evenly to be natural spiral down past the point "
        "where the walls stop echoing back.",
        {{"up", "sealed_vault"}, {"down", "abyss"}},
        {},
        std::nullopt,
        std::nullopt,
        {},
    };

    // The Nameless Thing is both `npcId` and `enemyId` here: `talk` opens a
    // conversation that can resolve the encounter without a fight (see
    // dialogue::npcRegistry's "nameless_thing" entry and ui::terminal's
    // post-talk quest-completion check, which mirrors the post-combat one
    // for the peaceful branch); `attack` still works exactly as any other
    // fight if the player chooses (or defaults into) that instead.
    w.locations_["abyss"] = Location{
        "abyss",
        "the Abyss",
        "There is no floor here that you can see, only the thing standing "
        "on it, and the sense that it has been waiting a very long time "
        "for someone to finally arrive.",
        {{"up", "abyssal_stair"}},
        {},
        std::string("nameless_thing"),
        std::string("nameless_thing"),
        {},
    };

    return w;
}

namespace {

// Adds `exit` to `loc` unless a same-direction exit already exists —
// reconcile() runs on every load plus after every quest-completing kill, so
// this keeps repeated calls from duplicating an already-opened passage.
void openExitOnce(Location &loc, const Exit &exit) {
    bool exists = std::any_of(
        loc.exits.begin(), loc.exits.end(),
        [&](const Exit &e) { return e.direction == exit.direction; });
    if (!exists) loc.exits.push_back(exit);
}

} // namespace

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
            // opens.
            openExitOnce(*undercroft, Exit{"down", "crypt_stair"});
        }
        quest::setFlag(progress, "quest.the_keepsake.started");
    }

    if (quest::hasFlag(progress, "quest.the_keepsake.complete")) {
        if (Location *vault = find("sealed_vault")) {
            vault->enemyId.reset();
            openExitOnce(*vault, Exit{"down", "abyssal_stair"});
        }
        quest::setFlag(progress, "quest.the_nameless.started");
    }

    // Resolved either by combat (combat::run + quest::onEnemyDefeated) or
    // peacefully (the dialogue branch that sets this flag directly) — see
    // dialogue::npcRegistry's "nameless_thing" entry. Either path clears
    // both roles so the room settles once the quest is done.
    if (quest::hasFlag(progress, "quest.the_nameless.complete")) {
        if (Location *abyss = find("abyss")) {
            abyss->enemyId.reset();
            abyss->npcId.reset();
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
