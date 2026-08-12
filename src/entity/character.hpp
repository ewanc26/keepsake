#ifndef KEEPSAKE_ENTITY_CHARACTER_HPP
#define KEEPSAKE_ENTITY_CHARACTER_HPP

#include <string>
#include <vector>

namespace keepsake::entity {

struct ItemStack {
    std::string itemId;
    int count = 1;
};

// A static item definition. `consumable` items are removed on use and heal
// `healAmount`; non-consumable items with a nonzero `attackBonus` are a
// one-time permanent upgrade applied (and then removed from the inventory)
// on use — there is no equipment-slot system in Phase 1, just this one-shot
// shortcut.
struct ItemDef {
    std::string id;
    std::string name;
    std::string description;
    bool consumable = false;
    int healAmount = 0;
    int attackBonus = 0;
};

const ItemDef *findItemDef(const std::string &id);
const std::vector<ItemDef> &allItemDefs();

struct Character {
    std::string className = "Wanderer";
    int level = 1;
    int xp = 0;
    int hp = 20;
    int maxHp = 20;
    int attack = 4;
    int defense = 1;
    // Set at character creation from identity::hash() — see
    // src/identity/identity.hpp. Phase 3 will use it to bias world
    // generation by account signal instead of just anchoring the save.
    std::string worldSeed;
    // Set once at character creation (see util::isoNow()); preserved
    // across saves so a click.croft.rpg.character record's createdAt
    // reflects when the character was actually created, not when it was
    // last synced. Empty for a character that predates this field.
    std::string createdAt;
    std::vector<ItemStack> inventory;

    void addItem(const std::string &itemId, int count = 1);
    // Returns false and leaves the inventory unchanged if there isn't
    // enough of the item.
    bool removeItem(const std::string &itemId, int count = 1);
    int itemCount(const std::string &itemId) const;

    // Applies XP and handles level-up (flat curve: level * 20 XP to the
    // next level, +5 max HP and +1 attack/defense per level).
    void gainXp(int amount);
};

} // namespace keepsake::entity

#endif
