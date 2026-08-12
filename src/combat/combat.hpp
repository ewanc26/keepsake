#ifndef KEEPSAKE_COMBAT_COMBAT_HPP
#define KEEPSAKE_COMBAT_COMBAT_HPP

#include <ostream>
#include <string>
#include <vector>

#include "entity/character.hpp"

namespace keepsake::combat {

struct EnemyDef {
    std::string id;
    std::string name;
    int hp = 1;
    int attack = 1;
    int defense = 0;
    int xpReward = 0;
    // Empty means no loot.
    std::string lootItemId;
};

const EnemyDef *findEnemyDef(const std::string &id);

enum class Result { Victory, Defeat };

// Runs a full turn-based fight to conclusion, narrating each exchange to
// `out`. Mutates `player` (HP loss, XP/loot on victory) in place.
Result run(entity::Character &player, const EnemyDef &enemy, std::ostream &out);

} // namespace keepsake::combat

#endif
