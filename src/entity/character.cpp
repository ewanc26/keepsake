#include "entity/character.hpp"

#include <algorithm>

namespace keepsake::entity {

namespace {

const std::vector<ItemDef> &itemRegistry() {
    static const std::vector<ItemDef> items = {
        {"health_potion", "health potion",
         "A small clay vial, still cool to the touch.", true, 15, 0, 0},
        {"iron_sword", "iron sword",
         "Notched and rust-spotted, but the edge still bites.", false, 0, 3,
         0},
        {"rusty_dagger", "rusty dagger",
         "Light and quick, though the rust has eaten into the blade.",
         false, 0, 1, 0},
        {"leather_armor", "leather armor",
         "Cracked and stiff with age, but it still turns a blow.", false, 0,
         0, 2},
        {"silver_locket", "silver locket",
         "Tarnished silver, warm despite the cold down here. Tilted to "
         "the light, the hollow inside holds a shape you half remember "
         "carving over a gate you have never seen before tonight: yours. "
         "It remembers names, and it has begun with yours.",
         false, 0, 0, 0},
        {"silk_gloves", "silk gloves",
         "Woven from spider-silk, near-weightless, and stronger than they "
         "look.", false, 0, 0, 1},
        // Belfry loot (side quest "the_bell") — carried off by the crow
        // with the rest of the tower's brass.
        {"brass_spyglass", "brass spyglass",
         "Dented and green with age, but the lenses still find their "
         "mark. Part of the tower's brass, once.", false, 0, 2, 0},
        {"steel_sword", "steel sword",
         "Unlike anything else down here, kept oiled and sharp. Someone "
         "wanted this to last.", false, 0, 6, 0},
        {"scale_mail", "scale mail",
         "Overlapping plates, cold and heavy, scarred by whatever wore it "
         "last.", false, 0, 0, 5},
        {"health_elixir", "health elixir",
         "Thicker and darker than a potion, and it smells like copper.",
         true, 35, 0, 0},
        {"ring_of_the_watcher", "ring of the Watcher",
         "A plain iron band, worn smooth. It was old before the Watcher "
         "ever held it.", false, 0, 4, 4},
        // Readable lore notes (see ItemDef::readable). None are placed by
        // the default world — they surface through the worldSeed's seeded
        // cache spots in world.cpp, so each identity's world tells its own
        // fragment of the keep's story.
        // The premise (see README's Concept and the Watcher's dialogue):
        // every world opens with the open letter waiting in the gatehouse
        // — no seed gates it — which names the mystery, the stakes, and
        // the DID hook before the player's first turn is over.
        {"note.open_letter", "open letter",
         "To whoever is holding this: I am sorry. A door below opened on "
         "my watch, because a door must, eventually, open for somebody. "
         "The keepsake they carried off is a locket of the old make — the "
         "kind that remembers names, and the kind that must not be taught "
         "a wrong one. It was not taken for its silver. It was taken by "
         "something that lost its own name before your grandparents were "
         "born, and understands nothing except that the world remembers "
         "everyone except it, and it is finished with that. You will find "
         "a line of characters cut over the gate that belongs to you and "
         "nobody else — signed, sealed, or dreaming, it was waiting here "
         "before you were. That mark is your claim on this place, and "
         "teaching it to the locket is your protection, and its whole "
         "danger. Do not let the thing below learn it first. It will "
         "begin politely — they always begin politely. — the last "
         "keeper's hand", false, 0, 0, 0, true},
        {"note.watch_log", "watch log page",
         "Night 400-odd. The bell went quiet at moonset and nobody went "
         "up. I told them it was a bird; I have been telling myself that "
         "for a long time now. The Knight no longer answers when I knock "
         "on the undercroft door.", false, 0, 0, 0, true},
        {"note.crypt_ledger", "crypt keeper's ledger",
         "Interment, final: one of the Wardens, name struck through by "
         "the Warden's own hand. Fee marked paid in full. Below, in a "
         "different, older ink: 'We keep what keeps the door. Do not "
         "count the stock.'", false, 0, 0, 0, true},
        {"note.abyssal_inscription", "abyssal stair inscription",
         "The steps are cut in something too old to read outright, but "
         "one line fits any language you know: 'It is not the depth that "
         "humiliates. It is that the door was opened for a reason, once, "
         "and from the inside.'", false, 0, 0, 0, true},
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
