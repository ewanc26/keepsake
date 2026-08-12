#include "combat/combat.hpp"

#include <algorithm>
#include <random>

namespace keepsake::combat {

namespace {

const std::vector<EnemyDef> &enemyRegistry() {
    static const std::vector<EnemyDef> enemies = {
        {"hollow_knight", "the Hollow Knight", 24, 4, 1, 45, ""},
    };
    return enemies;
}

std::mt19937 &rng() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

int rollDamage(int attack, int defense) {
    std::uniform_int_distribution<int> variance(-2, 2);
    int base = attack - defense + variance(rng());
    return std::max(1, base);
}

} // namespace

const EnemyDef *findEnemyDef(const std::string &id) {
    const auto &enemies = enemyRegistry();
    auto it = std::find_if(enemies.begin(), enemies.end(),
                           [&](const EnemyDef &e) { return e.id == id; });
    return it == enemies.end() ? nullptr : &(*it);
}

Result run(entity::Character &player, const EnemyDef &enemyDef,
           std::ostream &out) {
    int enemyHp = enemyDef.hp;

    out << "You square off against " << enemyDef.name << ".\n";

    while (player.hp > 0 && enemyHp > 0) {
        int dealt = rollDamage(player.attack, enemyDef.defense);
        enemyHp = std::max(0, enemyHp - dealt);
        out << "You strike " << enemyDef.name << " for " << dealt
            << " damage. (" << enemyHp << "/" << enemyDef.hp << " HP)\n";
        if (enemyHp == 0) break;

        int taken = rollDamage(enemyDef.attack, player.defense);
        player.hp = std::max(0, player.hp - taken);
        out << enemyDef.name << " strikes back for " << taken << " damage. ("
            << player.hp << "/" << player.maxHp << " HP)\n";
    }

    if (player.hp == 0) {
        out << "\nYou collapse. Everything goes dark.\n";
        return Result::Defeat;
    }

    out << "\n" << enemyDef.name << " falls.\n";
    player.gainXp(enemyDef.xpReward);
    out << "You gain " << enemyDef.xpReward << " XP.\n";
    if (!enemyDef.lootItemId.empty()) {
        player.addItem(enemyDef.lootItemId);
        out << "It drops something.\n";
    }
    return Result::Victory;
}

} // namespace keepsake::combat
