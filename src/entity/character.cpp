#include "entity/character.hpp"

#include <algorithm>

namespace keepsake::entity {

namespace {

const std::vector<ItemDef> &itemRegistry() {
    static const std::vector<ItemDef> items = {
        {"health_potion", "health potion",
         "A small clay vial, still cool to the touch.", true, 15, 0},
        {"iron_sword", "iron sword",
         "Notched and rust-spotted, but the edge still bites.", false, 0, 3},
    };
    return items;
}

} // namespace

const ItemDef *findItemDef(const std::string &id) {
    const auto &items = itemRegistry();
    auto it = std::find_if(items.begin(), items.end(),
                           [&](const ItemDef &d) { return d.id == id; });
    return it == items.end() ? nullptr : &(*it);
}

const std::vector<ItemDef> &allItemDefs() {
    return itemRegistry();
}

void Character::addItem(const std::string &itemId, int count) {
    for (auto &stack : inventory) {
        if (stack.itemId == itemId) {
            stack.count += count;
            return;
        }
    }
    inventory.push_back(ItemStack{itemId, count});
}

bool Character::removeItem(const std::string &itemId, int count) {
    for (auto it = inventory.begin(); it != inventory.end(); ++it) {
        if (it->itemId != itemId) continue;
        if (it->count < count) return false;
        it->count -= count;
        if (it->count == 0) inventory.erase(it);
        return true;
    }
    return false;
}

int Character::itemCount(const std::string &itemId) const {
    for (const auto &stack : inventory) {
        if (stack.itemId == itemId) return stack.count;
    }
    return 0;
}

void Character::gainXp(int amount) {
    xp += amount;
    int need = level * 20;
    while (xp >= need) {
        xp -= need;
        ++level;
        maxHp += 5;
        hp = maxHp;
        ++attack;
        ++defense;
        need = level * 20;
    }
}

} // namespace keepsake::entity
