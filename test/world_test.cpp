// Unit tests for world::World — the location graph and its one piece of
// dynamic behavior: reconcile() opening the crypt stair once keep_cleared
// completes.

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "world/world.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at world_test.cpp:" << __LINE__ << "\n";            \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using namespace keepsake::world;
    using keepsake::quest::Progress;

    // createDefault() wires up every location this phase ships with.
    {
        World w = World::createDefault();
        for (const char *id : {"gatehouse", "courtyard", "armory", "chapel",
                               "watchtower_base", "watchtower_top",
                               "undercroft", "crypt_stair", "crypt",
                               "bone_chamber", "collapsed_passage",
                               "spider_den", "flooded_tunnel", "sealed_vault",
                               "abyssal_stair", "abyss"}) {
            CHECK(w.find(id) != nullptr);
        }
    }

    // The watchtower hangs off the courtyard's west side and links
    // through, with the crow waiting at the top.
    {
        World w = World::createDefault();
        const Location *courtyard = w.find("courtyard");
        const Location *base = w.find("watchtower_base");
        const Location *top = w.find("watchtower_top");
        CHECK(courtyard != nullptr && base != nullptr && top != nullptr);
        bool hasWest = false;
        for (const auto &exit : courtyard->exits) {
            if (exit.direction == "west" && exit.targetId == "watchtower_base")
                hasWest = true;
        }
        CHECK(hasWest);
        CHECK(base->exits.size() == 2); // east + up
        CHECK(top->npcId == std::nullopt);
        CHECK(top->enemyId == std::string("carrion_crow"));
    }

    // Reconcile: once the bell quest completes, the belfry's crow stands
    // down and its description reflects the rung bell — and stays
    // consistent across repeated reconcile() calls.
    {
        World w = World::createDefault();
        Progress p;
        keepsake::quest::setFlag(p, "quest.the_bell.complete");
        w.reconcile(p);
        w.reconcile(p);

        const Location *top = w.find("watchtower_top");
        CHECK(top != nullptr);
        CHECK(!top->enemyId.has_value());
        CHECK(top->description.find("hangs clean again") != std::string::npos);
    }

    // Reconcile: an incomplete bell quest never touches the belfry.
    {
        World w = World::createDefault();
        Progress p;
        w.reconcile(p);
        const Location *top = w.find("watchtower_top");
        CHECK(top != nullptr);
        CHECK(top->enemyId == std::string("carrion_crow"));
    }

    // Before keep_cleared, the undercroft has no way down.
    {
        World w = World::createDefault();
        Progress p;
        w.reconcile(p);

        const Location *undercroft = w.find("undercroft");
        CHECK(undercroft != nullptr);
        bool hasDown = false;
        for (const auto &exit : undercroft->exits) {
            if (exit.direction == "down") hasDown = true;
        }
        CHECK(!hasDown);
        CHECK(!keepsake::quest::hasFlag(p, "quest.the_keepsake.started"));
    }

    // Once keep_cleared completes, reconcile() clears the boss, opens the
    // stair down, and starts the_keepsake — exactly once, not duplicated on
    // repeated reconcile() calls (e.g. across multiple saves/loads).
    {
        World w = World::createDefault();
        Progress p;
        keepsake::quest::setFlag(p, "quest.keep_cleared.complete");

        w.reconcile(p);
        w.reconcile(p);

        const Location *undercroft = w.find("undercroft");
        CHECK(undercroft != nullptr);
        CHECK(!undercroft->enemyId.has_value());

        int downCount = 0;
        for (const auto &exit : undercroft->exits) {
            if (exit.direction == "down") ++downCount;
        }
        CHECK(downCount == 1);
        CHECK(keepsake::quest::hasFlag(p, "quest.the_keepsake.started"));
    }

    // Once the_keepsake completes too, the vault's boss clears and the
    // abyssal stair opens — same "exactly once" guarantee.
    {
        World w = World::createDefault();
        Progress p;
        keepsake::quest::setFlag(p, "quest.keep_cleared.complete");
        keepsake::quest::setFlag(p, "quest.the_keepsake.complete");

        w.reconcile(p);
        w.reconcile(p);

        const Location *vault = w.find("sealed_vault");
        CHECK(vault != nullptr);
        CHECK(!vault->enemyId.has_value());

        int downCount = 0;
        for (const auto &exit : vault->exits) {
            if (exit.direction == "down") ++downCount;
        }
        CHECK(downCount == 1);
        CHECK(keepsake::quest::hasFlag(p, "quest.the_nameless.started"));
    }

    // The abyss's boss/NPC only clear once the_nameless completes, whether
    // that happened through combat or the peaceful dialogue branch — either
    // way reconcile() doesn't care which, only the flag.
    {
        World w = World::createDefault();
        Progress p;
        keepsake::quest::setFlag(p, "quest.the_nameless.complete");
        w.reconcile(p);

        const Location *abyss = w.find("abyss");
        CHECK(abyss != nullptr);
        CHECK(!abyss->enemyId.has_value());
        CHECK(!abyss->npcId.has_value());
    }

    // applyWorldSeed: deterministic — same seed, same world, caches and
    // flourish included.
    {
        for (const std::string &seed :
             {"local:0011223344556677", "did:plc:abcdef123456", ""}) {
            World a = World::createDefault();
            World b = World::createDefault();
            a.applyWorldSeed(seed);
            b.applyWorldSeed(seed);
            for (const char *id : {"gatehouse", "courtyard", "chapel",
                                   "crypt_stair", "collapsed_passage",
                                   "spider_den", "flooded_tunnel", "abyss"}) {
                const Location *la = a.find(id);
                const Location *lb = b.find(id);
                CHECK(la != nullptr && lb != nullptr);
                CHECK(la->itemIds == lb->itemIds);
                CHECK(la->description == lb->description);
            }
        }
    }

    // applyWorldSeed: actually varies — across a spread of seeds, more
    // than one distinct placement turns up (else the seed isn't shaping
    // the world at all).
    {
        std::vector<std::vector<std::string>> placements;
        std::vector<std::string> flourishes;
        for (int i = 0; i < 32; ++i) {
            World w = World::createDefault();
            w.applyWorldSeed("seed-" + std::to_string(i));
            std::vector<std::string> placement;
            std::string flourish;
            for (const Location *loc :
                 {w.find("gatehouse"), w.find("courtyard"), w.find("chapel"),
                  w.find("crypt_stair"), w.find("collapsed_passage"),
                  w.find("spider_den"), w.find("flooded_tunnel"),
                  w.find("sealed_vault")}) {
                CHECK(loc != nullptr);
                for (const auto &item : loc->itemIds) {
                    placement.push_back(loc->id + ":" + item);
                }
            }
            // Flourish detection: any description longer than the seeded
            // base means a line was appended there.
            World plain = World::createDefault();
            for (const char *id : {"gatehouse", "courtyard", "chapel",
                                   "crypt", "sealed_vault"}) {
                if (w.find(id)->description != plain.find(id)->description) {
                    flourish = id;
                }
            }
            placements.push_back(placement);
            flourishes.push_back(flourish);
        }
        std::sort(placements.begin(), placements.end());
        CHECK(std::unique(placements.begin(), placements.end()) -
                      placements.begin() >
              1);
        std::sort(flourishes.begin(), flourishes.end());
        CHECK(std::unique(flourishes.begin(), flourishes.end()) -
                      flourishes.begin() >
              1);
    }

    // applyMarkEchoes: the thing below has been learning the mark — its
    // practice appears on the way down, deterministic from the mark alone.
    {
        const std::string mark = "0123456789abcdef";
        World a = World::createDefault();
        a.applyMarkEchoes(mark);
        World b = World::createDefault();
        b.applyMarkEchoes(mark);

        // Deterministic: the same mark cuts the same echoes into the same
        // walls.
        for (const char *id : {"crypt_stair", "abyssal_stair", "abyss"}) {
            CHECK(a.find(id)->description == b.find(id)->description);
        }

        // The shallow echo is a short prefix of the mark, and the deeper
        // practice a longer one — each round less wrong.
        const std::string &shallow = a.find("crypt_stair")->description;
        std::string::size_type at = shallow.find("characters: ");
        CHECK(at != std::string::npos);
        if (at != std::string::npos) {
            std::string copied =
                shallow.substr(at + 12, shallow.find('.', at) - (at + 12));
            CHECK(copied.size() >= 3 && copied.size() <= 5);
            CHECK(mark.compare(0, copied.size(), copied) == 0);
        }

        // The learned echo is the full mark with exactly one character
        // wrong — never the mark itself, never more than one off.
        const std::string &abyss = a.find("abyss")->description;
        bool foundAlmost = false;
        for (std::string::size_type i = 0; i + mark.size() <= abyss.size(); ++i) {
            std::string window = abyss.substr(i, mark.size());
            int wrong = 0;
            for (std::string::size_type c = 0; c < mark.size(); ++c)
                wrong += window[c] != mark[c];
            if (wrong == 1) foundAlmost = true;
        }
        CHECK(foundAlmost);

        // A different identity finds different practice carved in.
        World c = World::createDefault();
        c.applyMarkEchoes("fedcba9876543210");
        CHECK(c.find("abyss")->description != abyss);
    }

    // applyMarkEchoes: idempotent, and a markless world stays unwritten.
    {
        World once = World::createDefault();
        once.applyMarkEchoes("0123456789abcdef");
        World twice = World::createDefault();
        twice.applyMarkEchoes("0123456789abcdef");
        twice.applyMarkEchoes("0123456789abcdef");
        CHECK(once.find("crypt_stair")->description ==
              twice.find("crypt_stair")->description);
        CHECK(once.find("abyss")->description ==
              twice.find("abyss")->description);

        World bare = World::createDefault();
        bare.applyMarkEchoes("");
        CHECK(bare.find("crypt_stair")->description ==
              World::createDefault().find("crypt_stair")->description);
    }

    // applyWorldSeed: idempotent — applying the same seed twice doesn't
    // duplicate caches or flourish lines.
    {
        World once = World::createDefault();
        once.applyWorldSeed("local:fedcba9876543210");
        World twice = World::createDefault();
        twice.applyWorldSeed("local:fedcba9876543210");
        twice.applyWorldSeed("local:fedcba9876543210");
        for (const char *id : {"gatehouse", "courtyard", "chapel", "crypt",
                               "sealed_vault"}) {
            CHECK(once.find(id)->description == twice.find(id)->description);
            CHECK(once.find(id)->itemIds == twice.find(id)->itemIds);
        }
    }

    // applyWorldSeed + reconcile: a taken seeded cache doesn't respawn, the
    // same way hand-placed items don't — take sets the generic
    // item.taken.<loc>.<item> flag, and a re-derived world with the same
    // seed reconciles it away.
    {
        World w = World::createDefault();
        w.applyWorldSeed("did:plc:0123456789abcdef");
        std::string seededItemLoc;
        std::string seededItemId;
        for (const char *id : {"gatehouse", "chapel", "crypt_stair",
                               "collapsed_passage", "spider_den",
                               "flooded_tunnel"}) {
            const Location *loc = w.find(id);
            if (!loc->itemIds.empty()) {
                seededItemLoc = id;
                seededItemId = loc->itemIds.front();
                break;
            }
        }
        CHECK(!seededItemLoc.empty() && !seededItemId.empty());

        Progress p;
        // Simulate the take handler's flag (ui::terminal's `take`).
        keepsake::quest::setFlag(
            p, "item.taken." + seededItemLoc + "." + seededItemId);

        World reloaded = World::createDefault();
        reloaded.applyWorldSeed("did:plc:0123456789abcdef");
        reloaded.reconcile(p);
        const Location *loc = reloaded.find(seededItemLoc);
        CHECK(loc != nullptr);
        bool stillThere = false;
        for (const auto &item : loc->itemIds) {
            if (item == seededItemId) stillThere = true;
        }
        CHECK(!stillThere);
    }

    // applyWorldSeed: across a spread of seeds, readable lore notes really
    // do surface — the chapel/crypt-stair/abyssal-stair cache pools carry
    // them alongside the consumables (deterministic given the fixed
    // splitmix64 generator, so this assertion is stable).
    {
        int worldsWithNotes = 0;
        for (int i = 0; i < 64; ++i) {
            World w = World::createDefault();
            w.applyWorldSeed("note-surface-seeds-" + std::to_string(i));
            for (const char *id : {"chapel", "crypt_stair", "abyssal_stair"}) {
                const Location *loc = w.find(id);
                CHECK(loc != nullptr);
                for (const auto &item : loc->itemIds) {
                    if (item.rfind("note.", 0) == 0) {
                        ++worldsWithNotes;
                        break;
                    }
                }
            }
        }
        CHECK(worldsWithNotes > 0);
    }

    // The open letter is premise content, never seed-gated: every
    // default world carries it in the gatehouse — before any seed is
    // applied, and after whichever seed applies — and it reacts to
    // reconcile()'s ordinary taken-item mechanics like anything else.
    {
        World plain = World::createDefault();
        CHECK(plain.find("gatehouse")->itemIds.size() == 1);
        CHECK(plain.find("gatehouse")->itemIds[0] == "note.open_letter");

        for (const std::string &seed : {"alpha", "beta", "gamma", "delta"}) {
            World w = World::createDefault();
            w.applyWorldSeed(seed);
            const Location *gatehouse = w.find("gatehouse");
            CHECK(gatehouse != nullptr);
            bool hasLetter = false;
            for (const auto &item : gatehouse->itemIds) {
                if (item == "note.open_letter") hasLetter = true;
            }
            CHECK(hasLetter);
        }

        // And once taken, it stays taken — the generic flag mechanics.
        Progress p;
        keepsake::quest::setFlag(p, "item.taken.gatehouse.note.open_letter");
        World reloaded = World::createDefault();
        reloaded.applyWorldSeed("alpha");
        reloaded.reconcile(p);
        CHECK(reloaded.find("gatehouse")->itemIds.empty());
    }

    std::cout << "world_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
