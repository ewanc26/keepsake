// Unit tests for combat::run. Damage rolls carry a std::random_device-seeded
// +/-2 variance (see combat.cpp's rollDamage), so cases are built with stat
// gaps wide enough that the outcome and iteration count are deterministic
// regardless of the actual roll — never asserting an exact damage number.

#include <iostream>
#include <sstream>

#include "combat/combat.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at combat_test.cpp:" << __LINE__ << "\n";           \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using namespace keepsake::combat;
    using keepsake::entity::Character;

    // findEnemyDef: known and unknown ids.
    {
        const EnemyDef *known = findEnemyDef("hollow_knight");
        CHECK(known != nullptr);
        if (known != nullptr) CHECK(known->name == "the Hollow Knight");

        CHECK(findEnemyDef("no_such_enemy") == nullptr);
    }

    // Victory: player's attack dwarfs the enemy's HP, so even with -2
    // variance the enemy dies on the first strike, before it can counter —
    // player.hp must be untouched, XP granted, loot added.
    {
        Character player;
        player.hp = player.maxHp = 20;
        player.attack = 1000;
        player.defense = 100;
        player.xp = 0;
        player.level = 1;

        EnemyDef enemy{"test_enemy",
                       "the Test Dummy",
                       /*hp=*/5,
                       /*attack=*/1,
                       /*defense=*/0,
                       /*xpReward=*/7,
                       /*lootItemId=*/"loot_test_item"};

        std::ostringstream out;
        Result result = run(player, enemy, out);

        CHECK(result == Result::Victory);
        CHECK(player.hp == 20);
        CHECK(player.itemCount("loot_test_item") == 1);
        // xpReward(7) < level-1's threshold(20), so no level-up — xp adds
        // directly.
        CHECK(player.xp == 7);
        CHECK(!out.str().empty());
    }

    // Victory with no loot: lootItemId empty means no item is granted.
    {
        Character player;
        player.hp = player.maxHp = 20;
        player.attack = 1000;
        player.defense = 100;

        EnemyDef enemy{"no_loot", "Loot-less Foe", 5, 1, 0, 3, ""};

        std::ostringstream out;
        run(player, enemy, out);
        CHECK(player.inventory.empty());
    }

    // Defeat: enemy's attack dwarfs the player's HP and the player's attack
    // can't kill the enemy in the one exchange it takes for the player to
    // die, so the fight ends in Defeat with player.hp floored at 0.
    {
        Character player;
        player.hp = player.maxHp = 5;
        player.attack = 1;
        player.defense = 0;

        EnemyDef enemy{"crusher",       "the Crusher", /*hp=*/50,
                       /*attack=*/1000, /*defense=*/0, /*xpReward=*/99, ""};

        std::ostringstream out;
        Result result = run(player, enemy, out);

        CHECK(result == Result::Defeat);
        CHECK(player.hp == 0);
        // No XP/loot on defeat.
        CHECK(player.xp == 0);
        CHECK(player.inventory.empty());
    }

    std::cout << "combat_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
