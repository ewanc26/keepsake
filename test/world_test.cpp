// Unit tests for world::World — the location graph and its one piece of
// dynamic behavior: reconcile() opening the crypt stair once keep_cleared
// completes.

#include <iostream>

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
                               "undercroft", "crypt_stair", "crypt",
                               "bone_chamber", "flooded_tunnel",
                               "sealed_vault"}) {
            CHECK(w.find(id) != nullptr);
        }
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

    std::cout << "world_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
