#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace keepsake::world {

World World::createDefault() {
    World w;

    w.locations_["gatehouse"] = Location{
        "gatehouse",
        "the Gatehouse",
        "The gate hangs open on one hinge, and it was not forced — "
        "whoever opened it was let in. A letter lies just past the "
        "threshold, weighted under a stone, dry despite the weather, as "
        "though the watch that ran this tower only just stepped inside.",
        {{"north", "courtyard"}},
        {"note.open_letter"},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["courtyard"] = Location{
        "courtyard",
        "the Courtyard",
        "Weeds grow between old flagstones. An old man sits near a "
        "cracked well, watching the gate.",
        {{"south", "gatehouse"}, {"east", "armory"}, {"north", "chapel"},
         {"west", "watchtower_base"}},
        {},
        std::string("watcher"),
        std::nullopt,
        {},
    };

    // The watchtower (side quest "the_bell", see quest.cpp and the
    // Watcher's "tower" dialogue node): reachable from the courtyard, but
    // the crow is purely optional content — nothing in the three main
    // quests routes through it.
    w.locations_["watchtower_base"] = Location{
        "watchtower_base",
        "the Watchtower Stair",
        "A narrow spiral climbs the inside of the keep's western tower. "
        "Wind comes down the stairwell smelling of marsh, and the bell "
        "rope, frayed to a whisp, dangles past you from far above.",
        {{"east", "courtyard"}, {"up", "watchtower_top"}},
        {},
        std::nullopt,
        std::nullopt,
        {},
    };

    w.locations_["watchtower_top"] = Location{
        "watchtower_top",
        "the Belfry",
        "Wind combs across the open roof, and the marshes run out to "
        "grey distance on every side. The keep's great bell hangs "
        "verdigrised in its frame, half-buried in a nest of sticks, "
        "bones, and stolen brass.",
        {{"down", "watchtower_base"}},
        {},
        std::nullopt,
        std::string("carrion_crow"),
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

namespace {

// -- Seeded PRNG ------------------------------------------------------------
//
// FNV-1a over the seed, stirred through splitmix64. No <random> machinery:
// every platform and standard-library build must derive the exact same
// world from the exact same worldSeed, so the generator and its constants
// are fixed here rather than delegated to an implementation-defined one.

std::uint64_t fnv1a(const std::string &text) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

class SeedRng {
  public:
    explicit SeedRng(std::uint64_t seed) : state_(seed) {}
    // splitmix64 — small, well-mixed, and trivially reproducible.
    std::uint64_t next() {
        state_ += 0x9e3779b97f4a7c15ULL;
        std::uint64_t x = state_;
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }
    std::size_t pick(std::size_t count) {
        return count == 0 ? 0 : static_cast<std::size_t>(next() % count);
    }

  private:
    std::uint64_t state_;
};

// Picks `k` distinct entries from `pool` in a seeded order (partial
// Fisher-Yates — deterministic without needing <algorithm>'s RNG overloads).
template <typename T>
std::vector<T> pickDistinct(SeedRng &rng, const std::vector<T> &pool,
                            std::size_t k) {
    std::vector<T> order = pool;
    std::size_t take = std::min(k, order.size());
    std::vector<T> chosen;
    chosen.reserve(take);
    for (std::size_t i = 0; i < take; ++i) {
        std::size_t j = i + rng.pick(order.size() - i);
        std::swap(order[i], order[j]);
        chosen.push_back(std::move(order[i]));
    }
    return chosen;
}

struct CacheSpot {
    const char *locationId;
    // Candidate items, seeded pick of one — only consumables and minor
    // gear, so a lucky cache can never skip a fight the quests gate.
    std::vector<const char *> items;
};

// Spots a cache can land in: rooms with no ordinary item otherwise, so a
// seeded cache never duplicates the hand-placed ones (and the `take`
// handler's flag-per-location convention keeps each from respawning).
const std::vector<CacheSpot> &cacheSpots() {
    static const std::vector<CacheSpot> spots = {
        {"gatehouse", {"health_potion"}},
        {"chapel", {"health_potion", "silk_gloves", "note.watch_log"}},
        {"crypt_stair", {"health_potion", "silk_gloves", "note.crypt_ledger"}},
        {"collapsed_passage", {"health_potion", "silk_gloves"}},
        {"spider_den", {"health_potion", "health_elixir", "silk_gloves"}},
        {"flooded_tunnel", {"health_potion", "health_elixir"}},
        {"abyssal_stair",
         {"health_potion", "note.abyssal_inscription"}},
    };
    return spots;
}

struct Flourish {
    const char *locationId;
    const char *line;
};

// One seeded line, appended to one location's description — the seed
// decides which world detail you happen to notice, nothing more.
const std::vector<Flourish> &flourishes() {
    static const std::vector<Flourish> all = {
        {"gatehouse",
         " Scratch-marks under the hinge could be claws — or a key, "
         "once, in a hurry."},
        {"courtyard",
         " The well is dry now, but the rope is still taut, as if "
         "someone just used it."},
        {"chapel",
         " One window still holds colored glass; the light through it "
         "falls in a blue band across the dust."},
        {"crypt",
         " One skull in the scatter is set apart from the others, "
         "facing the door."},
        {"sealed_vault",
         " Small clay bowls sit in careful rows along one wall, "
         "long since dried out."},
    };
    return all;
}

void addItemIdOnce(Location &loc, const std::string &itemId) {
    if (std::find(loc.itemIds.begin(), loc.itemIds.end(), itemId) ==
        loc.itemIds.end()) {
        loc.itemIds.push_back(itemId);
    }
}

} // namespace

void World::applyWorldSeed(const std::string &worldSeed) {
    SeedRng rng(fnv1a(worldSeed));
    // Two hidden caches, in a seeded pick of spot, each holding a seeded
    // pick of its candidate items.
    for (const CacheSpot &spot : pickDistinct(rng, cacheSpots(), 2)) {
        Location *loc = find(spot.locationId);
        if (loc == nullptr) continue;
        const auto &items = spot.items;
        addItemIdOnce(*loc, items[rng.pick(items.size())]);
    }

    // One description flourish, appended (never duplicated) to its
    // location.
    const std::vector<Flourish> &all = flourishes();
    const Flourish &flourish = all[rng.pick(all.size())];
    if (Location *loc = find(flourish.locationId)) {
        if (loc->description.find(flourish.line) == std::string::npos) {
            loc->description += flourish.line;
        }
    }
}

// The premise's follow-through (see the open letter's warning): the thing
// below has been learning your mark, and it practices the way anything
// practices — copying, wrong at first, less wrong each time. Deterministic
// from the mark itself rather than the worldSeed, because what it has
// learned is *you*: the same identity always finds the same copies of its
// own mark cut into the same walls. Derived here, not in main.cpp, so the
// progression (a shaky prefix, a longer one, then the full mark with one
// character wrong) is one authored thing with tests.

namespace {

std::string shakilyCopiedPrefix(SeedRng &rng, const std::string &mark,
                                std::size_t lo, std::size_t hi) {
    std::size_t take = lo + rng.pick(hi - lo + 1);
    take = std::min(take, mark.size());
    return mark.substr(0, take);
}

// The full mark, with exactly one character replaced by a wrong one — the
// last tell that this was learned, not taught.
std::string almostMark(SeedRng &rng, const std::string &mark) {
    static const char *digits = "0123456789abcdef";
    std::string copy = mark;
    if (copy.empty()) return copy;
    std::size_t at = rng.pick(copy.size());
    char wrong = digits[rng.pick(16)];
    while (wrong == copy[at]) wrong = digits[(wrong - '0' + 1) % 16];
    copy[at] = wrong;
    return copy;
}

} // namespace

void World::applyMarkEchoes(const std::string &mark) {
    if (mark.empty()) return;

    SeedRng rng(fnv1a("echo:" + mark));

    // Append once, like applyWorldSeed's flourish: the echo is derived
    // identically each run, so its own opening words say whether it is
    // already on the wall.
    auto appendOnce = [](Location &loc, const char *marker,
                         std::string line) {
        if (loc.description.find(marker) == std::string::npos)
            loc.description += std::move(line);
    };

    // The shallow copy: far above the thing, half-guessed.
    if (Location *loc = find("crypt_stair")) {
        appendOnce(*loc, "Scratched into the wall at shoulder height",
            std::string(" Scratched into the wall at shoulder height, in "
                        "something that dried brown, is a short string of "
                        "characters: ") +
            shakilyCopiedPrefix(rng, mark, 3, 5) +
            ". The strokes are wrong — cut left to right by something "
            "that had to be told which way reading goes.");
    }

    // The practice wall: the copy has grown, and grown steadier.
    if (Location *loc = find("abyssal_stair")) {
        appendOnce(*loc, "Cut into the stairwell's wall, repeated",
            std::string(" Cut into the stairwell's wall, repeated down past "
                        "where your light reaches, each attempt steadier "
                        "than the last: ") +
            shakilyCopiedPrefix(rng, mark, 8, 12) +
            ". The latest attempt is almost clean.");
    }

    // The learned mark: all but one character right, and the one that's
    // wrong is wrong in a way you can't stop seeing.
    if (Location *loc = find("abyss")) {
        appendOnce(*loc, "And scored across the floor, over and over",
            std::string(" And scored across the floor, over and over, "
                        "patient as penance, is your own mark — ") +
            almostMark(rng, mark) +
            " — every character yours but one. It wanted you to see "
            "that it has been paying attention.");
    }
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

    // The side quest's crow never blocks the main line — see the
    // watchtower locations' comment in createDefault(). Once the bell is
    // reclaimed (see quest.cpp's onEnemyDefeated, "carrion_crow"), the
    // belfry settles and its description reflects it.
    if (quest::hasFlag(progress, "quest.the_bell.complete")) {
        if (Location *belfry = find("watchtower_top")) {
            belfry->enemyId.reset();
            belfry->description =
                "Wind combs across the open roof, and the marshes run out "
                "to grey distance on every side. The keep's great bell "
                "hangs clean again, pushed clear of its foul nest, and "
                "rings out when you test it — the keep's first word in "
                "years.";
        }
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
